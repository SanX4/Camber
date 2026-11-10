"""Static feature-payload parity; runtime device certification remains separate."""
import hashlib,json,re,zipfile,struct
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT=Path(__file__).resolve().parents[1]
version=ET.parse(ROOT/'Directory.Build.props').findtext('PropertyGroup/Version')
output=ROOT/'artifacts'/version
baseline=json.loads((ROOT/'tools/android-payload-baseline.json').read_text(encoding='utf-8'))
rows=[]
def defined_classes(data):
    count,offset=struct.unpack_from('<II',data,0x60)
    strings=struct.unpack_from('<I',data,0x3c)[0]
    types=struct.unpack_from('<I',data,0x44)[0]
    result=set()
    for index in range(count):
        class_index=struct.unpack_from('<I',data,offset+index*32)[0]
        descriptor=struct.unpack_from('<I',data,types+class_index*4)[0]
        position=struct.unpack_from('<I',data,strings+descriptor*4)[0]
        while data[position]&128:position+=1
        position+=1
        result.add(data[position:data.index(b'\0',position)].decode('utf-8'))
    return result
for apk in sorted((output/'android').glob('*-internal.apk')):
    with zipfile.ZipFile(apk) as z:
        classes=set().union(*(defined_classes(z.read(n)) for n in z.namelist() if n.endswith('.dex')))
        assert 'Lorg/jni_zero/JniZero;' in classes,'WebRTC native bootstrap class was removed by R8.'
        assert 'Lorg/webrtc/PeerConnectionFactory;' in classes
        assert 'Lcom/google/common/flogger/FluentLogger;' in classes,'Stack-based logging factory must survive R8.'
        names=[n for n in z.namelist() if n.startswith('lib/') and n.endswith('.so')]
        architectures=sorted({n.split('/')[1] for n in names})
        assert names and set(names)=={n for n in baseline['nativeLibraries'] if n.split('/')[1] in architectures}
        assert all(hashlib.sha256(z.read(n)).hexdigest()==baseline['nativeLibraries'][n] for n in names)
        assert hashlib.sha256(z.read('assets/gesture_recognizer.task')).hexdigest()==baseline['gestureModelSha256']
        rows.append({'apk':apk.name,'bytes':apk.stat().st_size,'architectures':architectures,
            'nativeLibrariesIdenticalToBaseline':True,'gestureModelIdenticalToBaseline':True})
assert len(rows)==5,'Expected all four architectures and the universal fallback.'
with zipfile.ZipFile(next((output/'android').glob('*.aab'))) as z:
    assert all(hashlib.sha256(z.read('base/'+n)).hexdigest()==sha for n,sha in baseline['nativeLibraries'].items())
    assert hashlib.sha256(z.read('base/assets/gesture_recognizer.task')).hexdigest()==baseline['gestureModelSha256']
signature=(output/'reports/android-arm64-v8a-signature.txt').read_text(encoding='utf-8-sig')
assert re.search(r'Signer #1 certificate SHA-256 digest: (\w+)',signature).group(1)==baseline['developmentCertificateSha256']
report={'apks':rows,'completeAabPayloadPreserved':True,'developmentSigningIdentityMatchesPreviousDebugApk':True,
    'runtimeFeatureParity':'Static parity passed; execution and certification on each device architecture remain required.'}
(output/'reports/android-payload-parity.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('PASS five APKs and complete AAB: unchanged native libraries, gesture model and development signing identity.')
