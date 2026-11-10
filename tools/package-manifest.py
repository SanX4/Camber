"""Publish one local artifact index. Never installs, signs, uploads, or changes user data."""
import hashlib,os
import json
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
definition=ET.parse(ROOT/'Directory.Build.props')
version=definition.findtext('PropertyGroup/Version')
code=int(definition.findtext('PropertyGroup/AndroidVersionCode'))
output=ROOT/'artifacts'/version
def digest(path):
    with path.open('rb') as source:return hashlib.file_digest(source,'sha256').hexdigest()
files=[]
for kind in ('windows','windows-installer','android'):
    for path in sorted((output/kind).rglob('*')):
        if path.is_file():files.append({'path':path.relative_to(output).as_posix(),'bytes':path.stat().st_size,'sha256':digest(path)})
document={'version':version,'androidVersionCode':code,'distributionApproved':False,
    'status':'Internal development build; physical, Windows isolation, installation, security, signing and owner-approval gates remain.',
    'windowsLauncher':'windows/Camber.Desktop.exe' if (output/'windows/Camber.Desktop.exe').exists() else None,
    'windowsInstaller':f'windows-installer/Camber-Setup-{version}.exe' if (output/f'windows-installer/Camber-Setup-{version}.exe').exists() else None,
    'recommendedAndroidApk':f'android/Camber-{version}-arm64-v8a-internal.apk' if (output/f'android/Camber-{version}-arm64-v8a-internal.apk').exists() else None,
    'files':files}
output.mkdir(parents=True,exist_ok=True)
target=output/'build-manifest.json';temporary=target.with_name(target.name+f'.{os.getpid()}.partial')
temporary.write_text(json.dumps(document,indent=2)+'\n',encoding='utf-8');temporary.replace(target)
latest=ROOT/'artifacts/latest-build.json'
latestTemporary=latest.with_name(latest.name+f'.{os.getpid()}.partial')
latestTemporary.write_text(json.dumps({'version':version,'directory':version,'manifest':f'{version}/build-manifest.json','distributionApproved':False},indent=2)+'\n',encoding='utf-8')
latestTemporary.replace(latest)
print(f'Indexed {len(files)} development package files for {version}.')
