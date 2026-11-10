"""Compare matching package payloads; never substitute an older installed app's size."""
import argparse,json
from pathlib import Path
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--baseline',type=Path,required=True);args=parser.parse_args()
records=json.loads(args.baseline.read_text(encoding='utf-8'))['files']
version=ET.parse(ROOT/'Directory.Build.props').findtext('PropertyGroup/Version')
output=ROOT/'artifacts'/version
def total(directory):return sum(p.stat().st_size for p in directory.rglob('*') if p.is_file())
before=sum(r['bytes'] for r in records if r['path'].startswith('artifacts/ownership-proof/windows/'))
after=total(output/'windows')
arm=next((output/'android').glob('*-arm64-v8a-internal.apk')).stat().st_size
report={'unit':'bytes; MiB = bytes / 1048576',
    'windows':{'baseline':'0.8 ownership-proof development package; same offline self-contained policy',
        'baselinePayloadBytes':before,'newPayloadBytes':after,'reductionPercent':100*(1-after/before),
        'fifteenPercentTargetPassed':after<=before*.85,
        'installerBytes':next((output/'windows-installer').glob('*.exe')).stat().st_size,
        'actualNewInstalledSize':None,'installedSizeNote':'Not installed on this PC. Payload is unpacked application files; installer adds a separate shared C++ runtime when needed.'},
    'android':{'arm64InternalBytes':arm,'fortyMiBTargetPassed':arm<=40*1024**2,
        'packages':[{ 'name':p.name,'bytes':p.stat().st_size} for p in sorted((output/'android').iterdir()) if p.is_file()]},
    'legacyCopies':{'separateFolder':'F:\\Camber','installedCopy':'C:\\Program Files\\Camber','changed':False}}
(output/'reports/size-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(f'Windows payload: {before/1024**2:.2f} -> {after/1024**2:.2f} MiB ({100*(1-after/before):.1f}% smaller).')
print(f'ARM64 internal APK: {arm/1024**2:.2f} MiB. New installed size remains unmeasured.')
