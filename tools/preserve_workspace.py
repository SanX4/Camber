"""Lossless, verified archiving of historical build files. Never cleans source.

Archives are private local backups, not distribution artifacts. Cleanup removes
only unchanged, verified files recorded under the workspace's artifacts folder.
Reparse points, signing material and NTFS alternate streams remain untouched.
"""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def protected(path):
    if path.is_symlink() or path.stat().st_file_attributes & 0x400:
        return True
    if path.suffix.lower() in ('.pfx', '.jks', '.keystore', '.key', '.pem'):
        return True
    if os.name == 'nt':
        class StreamData(ctypes.Structure):
            _fields_ = [('size', ctypes.c_longlong), ('name', ctypes.c_wchar * 296)]
        kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        kernel.FindFirstStreamW.restype = ctypes.c_void_p
        kernel.FindFirstStreamW.argtypes = [ctypes.c_wchar_p, ctypes.c_int, ctypes.POINTER(StreamData), ctypes.c_ulong]
        kernel.FindNextStreamW.argtypes = [ctypes.c_void_p, ctypes.POINTER(StreamData)]
        kernel.FindClose.argtypes = [ctypes.c_void_p]
        data = StreamData()
        handle = kernel.FindFirstStreamW(str(path), 0, ctypes.byref(data), 0)
        if handle == ctypes.c_void_p(-1).value:
            return ctypes.get_last_error() not in (38, 2)  # no stream / missing file
        try:
            while True:
                if data.name != '::$DATA':
                    return True
                if not kernel.FindNextStreamW(handle, ctypes.byref(data)):
                    return ctypes.get_last_error() != 38
        finally:
            kernel.FindClose(handle)
    return False


def checked_path(name):
    relative = Path(name)
    if relative.is_absolute() or '..' in relative.parts or relative.parts[0] != 'artifacts':
        raise ValueError('Historical artifacts only: ' + name)
    path = ROOT / relative
    path.resolve().relative_to((ROOT / 'artifacts').resolve())
    return path


def verify(archive, manifest):
    with zipfile.ZipFile(archive) as z:
        for record in manifest['files']:
            with z.open(record['path']) as stream:
                if hashlib.file_digest(stream, 'sha256').hexdigest() != record['sha256']:
                    raise ValueError('Archive verification failed: ' + record['path'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('archive', 'verify', 'clean', 'restore'))
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--execute', action='store_true')
    parser.add_argument('--destination', type=Path)
    parser.add_argument('--exclude', action='append', default=[])
    parser.add_argument('--report',type=Path)
    args = parser.parse_args()
    directory = args.directory.resolve()
    archive, manifest_path = directory / 'historical-builds.zip', directory / 'historical-manifest.json'
    if args.action == 'archive':
        if archive.exists() or manifest_path.exists():
            raise ValueError('Never overwrite a historical archive')
        directory.mkdir(parents=True, exist_ok=True)
        records, kept = [], []
        files = sorted((ROOT / 'artifacts').rglob('*'))
        with zipfile.ZipFile(archive.with_suffix('.partial'), 'w', zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as z:
            for path in files:
                if not path.is_file():
                    continue
                name = path.relative_to(ROOT).as_posix()
                if any(name == prefix or name.startswith(prefix.rstrip('/') + '/') for prefix in args.exclude):
                    continue
                if protected(path):
                    kept.append(name)
                    continue
                stat = path.stat()
                sha = digest(path)
                z.write(path, name)
                if digest(path) != sha or path.stat().st_mtime_ns != stat.st_mtime_ns:
                    raise ValueError('File changed while archiving: ' + name)
                records.append({'path': name, 'sha256': sha, 'bytes': stat.st_size,
                                'mtimeNs': stat.st_mtime_ns, 'mode': stat.st_mode,
                                'attributes': stat.st_file_attributes})
                if len(records) % 150 == 0:
                    print(f'Archived {len(records)} files', flush=True)
        archive.with_suffix('.partial').replace(archive)
        manifest = {'version': 1, 'verified': False, 'files': records, 'preservedInPlace': kept}
        verify(archive, manifest)
        manifest['verified'] = True
        manifest_path.write_text(json.dumps(manifest, indent=2), encoding='utf-8')
        print(f'Verified {len(records)} archived files; {len(kept)} protected files kept in place.', flush=True)
        return
    manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    verify(archive, manifest)
    if args.action == 'verify':
        print('All archived content hashes verified.')
        return
    if args.action == 'restore':
        if not args.destination:
            parser.error('--destination is required; restoration never overwrites originals')
        destination = args.destination.resolve()
        destination.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(archive) as z:
            for record in manifest['files']:
                checked_path(record['path'])
                target = destination / record['path']
                target.resolve().relative_to(destination)
                if target.exists():
                    raise ValueError('Refusing to overwrite: ' + str(target))
                target.parent.mkdir(parents=True, exist_ok=True)
                with z.open(record['path']) as src, target.open('xb') as dst:
                    import shutil
                    shutil.copyfileobj(src, dst)
                if digest(target) != record['sha256']:
                    raise ValueError('Restore hash mismatch')
                os.utime(target, ns=(record['mtimeNs'], record['mtimeNs']))
                if os.name == 'nt':
                    # Restore ordinary file attributes, excluding unsupported device/reparse bits.
                    ctypes.windll.kernel32.SetFileAttributesW(str(target), record['attributes'] & 0x2027 or 0x80)
        print('Restored archived files with verified content.')
        return
    eligible = []
    for record in manifest['files']:
        path = checked_path(record['path'])
        if path.is_file() and not protected(path) and path.stat().st_mtime_ns==record['mtimeNs'] and digest(path) == record['sha256']:
            eligible.append((path,record))
    print(('Removing' if args.execute else 'Would remove'), len(eligible), 'verified historical files.')
    if args.execute:
        removed,kept=[],[]
        for path,record in eligible:
            try:
                if protected(path) or path.stat().st_mtime_ns!=record['mtimeNs']:
                    kept.append(record['path']);continue
                path.unlink();removed.append(record)
            except OSError:
                kept.append(record['path']) # Never stop a live application to remove a historical copy.
        # Only empty directories below artifacts; no recursive deletion.
        parents=set()
        for record in removed:
            parent=checked_path(record['path']).parent
            while parent!=(ROOT/'artifacts'):
                parents.add(parent);parent=parent.parent
        for path in sorted(parents,key=lambda p:len(p.parts),reverse=True):
            if path.is_dir() and not path.is_symlink() and not path.stat().st_file_attributes & 0x400:
                try:
                    path.rmdir()
                except OSError:
                    pass
        if args.report:
            args.report.parent.mkdir(parents=True,exist_ok=True)
            args.report.write_text(json.dumps({'removedFiles':len(removed),'removedBytes':sum(r['bytes'] for r in removed),
                'keptInUseOrChanged':kept,'archiveVerified':True,'sourceDeleted':False},indent=2),encoding='utf-8')


if __name__ == '__main__':
    main()
