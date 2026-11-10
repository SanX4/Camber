"""Fail-closed beta evidence gate; never publishes or installs anything."""
import argparse,json,sys,zipfile,struct,subprocess,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
REQUIRED=('source','runtime','android16k','recognition','ui','privacy','crossUserIsolation','independentSecurityReview','videoPerformance','endurance','compatibility','installer','windowsSigning','androidSigning','sbom','distributionSetup','ownerApproval')
def elf_check(apk):
 rows=[]
 with zipfile.ZipFile(apk) as z,open(apk,'rb') as raw:
  for info in z.infolist():
   if not info.filename.startswith(('lib/arm64-v8a/','lib/x86_64/')) or not info.filename.endswith('.so'):continue
   data=z.read(info)
   if data[:5]!=b'\x7fELF\x02':raise ValueError('Unexpected 64-bit ELF: '+info.filename)
   off=struct.unpack_from('<Q',data,32)[0];size,count=struct.unpack_from('<HH',data,54)
   align=[struct.unpack_from('<Q',data,off+i*size+48)[0] for i in range(count) if struct.unpack_from('<I',data,off+i*size)[0]==1]
   raw.seek(info.header_offset);header=raw.read(30);name,extra=struct.unpack_from('<HH',header,26)
   zip_ok=info.compress_type!=zipfile.ZIP_STORED or (info.header_offset+30+name+extra)%16384==0
   rows.append({'library':info.filename,'elf16k':bool(align) and min(align)>=16384,'zip16k':zip_ok})
 if not rows:raise ValueError('No supported 64-bit native libraries found')
 return rows
parser=argparse.ArgumentParser();parser.add_argument('--apk',type=Path);parser.add_argument('--check-apk-only',action='store_true');parser.add_argument('--evidence',type=Path,default=ROOT/'artifacts/beta/evidence.json');a=parser.parse_args()
blocked=[]
if a.apk:
 try:
  rows=elf_check(a.apk);print(json.dumps(rows,indent=2))
  if any(not r['elf16k'] or not r['zip16k'] for r in rows):blocked.append('android16k')
 except Exception as error:blocked.append('android16k: '+str(error))
if a.check_apk_only:
 if not a.apk:parser.error('--apk is required')
 sys.exit(2 if blocked else 0)
try:
 evidence=json.loads(a.evidence.read_text());checks=evidence['checks']
except (OSError,ValueError,KeyError):evidence={};checks={}
from beta_evidence import evaluate
head_result=subprocess.run(['git','rev-parse','--verify','HEAD'],cwd=ROOT,capture_output=True,text=True)
head=head_result.stdout.strip() if head_result.returncode==0 else None
status=subprocess.run(['git','status','--porcelain','--untracked-files=all'],cwd=ROOT,capture_output=True,text=True)
blocked+=evaluate(ROOT,evidence,head,status.returncode!=0 or bool(status.stdout.strip()))
if blocked:
 print('NOT READY FOR DISTRIBUTION\n'+'\n'.join('- '+s for s in blocked));sys.exit(2)
print('Evidence gate passed. Human approval of exact signed artifacts is still required before distribution.')
