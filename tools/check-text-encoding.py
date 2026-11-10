"""Enforce UTF-8 on repository text without scanning caches, backups or credentials."""
from pathlib import Path
import subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
names=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard','-z'],cwd=ROOT).split(b'\0')
extensions={'.kt','.kts','.cs','.csproj','.props','.json','.xml','.xaml','.cpp','.h','.hlsl','.in','.rc','.iss','.py','.ps1','.yml','.md','.csv','.txt','.properties'}
errors=[];count=0
for raw in names:
    if not raw:continue
    path=ROOT/raw.decode('utf-8')
    if path.suffix.lower() not in extensions:continue
    count+=1
    try:text=path.read_text(encoding='utf-8-sig')
    except UnicodeError:errors.append(str(path.relative_to(ROOT))+': not UTF-8');continue
    if '\ufffd' in text:errors.append(str(path.relative_to(ROOT))+': replacement character')
    if path.suffix.lower() in ('.kt','.cs') and any(s in text for s in ('\u00e2\u20ac','\u00e2\u0153','\u00f0\u0178','\u00c3\u00a9')):
        errors.append(str(path.relative_to(ROOT))+': damaged display text')
if errors:print('\n'.join(errors));sys.exit(1)
print(f'PASS UTF-8 encoding for {count} source/document files.')
