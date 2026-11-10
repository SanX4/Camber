"""Create a reviewable CycloneDX component inventory from locked dependencies.

This inventories components; license and vulnerability review remain separate gates.
It does not claim complete native transitive provenance or a verified dependency graph.
"""
import argparse
import base64
import hashlib
import json
import uuid
import xml.etree.ElementTree as ET
from pathlib import Path
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]
components = {}


def add(group, name, version, ecosystem, **extra):
    purl = f'pkg:{ecosystem}/' + (quote(group, safe='.') + '/' if group else '') + quote(name) + '@' + quote(version)
    item = {'type': 'library', 'bom-ref': purl, 'name': name, 'version': version, 'purl': purl}
    if group:
        item['group'] = group
    item.update(extra)
    components[purl] = item


for lock in (ROOT / 'src').glob('*/packages.lock.json'):
    for framework in json.loads(lock.read_text())['dependencies'].values():
        for name, info in framework.items():
            if info.get('type') == 'Project':
                continue
            extra = {}
            if info.get('contentHash'):
                extra['hashes'] = [{'alg': 'SHA-512', 'content': base64.b64decode(info['contentHash']).hex()}]
            add('', name, info['resolved'], 'nuget', **extra)

for line in (ROOT / 'mobile/android/app/gradle.lockfile').read_text().splitlines():
    coordinate = line.split('=', 1)[0]
    if line.startswith('#') or coordinate.count(':') != 2:
        continue
    group, name, version = coordinate.split(':')
    add(group, name, version, 'maven')

metadata = ET.parse(ROOT / 'mobile/android/gradle/verification-metadata.xml')
for node in metadata.findall('.//{*}component'):
    group, name, version = (node.attrib[key] for key in ('group', 'name', 'version'))
    purl = f'pkg:maven/{quote(group, safe=".")}/{quote(name)}@{quote(version)}'
    if purl in components:
        components[purl]['properties'] = [
            {'name': 'camber:artifact-sha256:' + artifact.attrib['name'], 'value': checksum.attrib['value']}
            for artifact in node.findall('{*}artifact') for checksum in artifact.findall('{*}sha256')]

manifest = json.loads((ROOT / 'tools/dependencies.json').read_text())
add('microsoft', 'DirectXTK', manifest['directXTK']['commit'], 'github', licenses=[{'license': {'id': 'MIT'}}])
add('nlohmann', 'json', manifest['nlohmannJsonCommit'], 'github', licenses=[{'license': {'id': 'MIT'}}])
add('webrtc-sdk', 'libwebrtc-windows', manifest['libwebrtcWindows']['tag'], 'generic',
    hashes=[{'alg': 'SHA-256', 'content': manifest['libwebrtcWindows']['sha256']}])
add('google', 'gesture-recognizer-model', 'float16-1', 'generic',
    hashes=[{'alg': 'SHA-256', 'content': manifest['gestureModel']['sha256']}])
add('microsoft', 'dotnet-sdk', manifest['dotnetSdk'], 'generic')
add('microsoft', 'dotnet-runtime', manifest['dotnetRuntime'], 'generic')
add('microsoft', 'aspnetcore-runtime', manifest['dotnetRuntime'], 'generic')
add('microsoft', 'windowsdesktop-runtime', manifest['dotnetRuntime'], 'generic')
items = [components[key] for key in sorted(components)]
digest = hashlib.sha256(json.dumps(items, sort_keys=True).encode()).hexdigest()
document = {'bomFormat': 'CycloneDX', 'specVersion': '1.6',
            'serialNumber': 'urn:uuid:' + str(uuid.uuid5(uuid.NAMESPACE_URL, digest)), 'version': 1,
            'metadata': {'component': {'type': 'application', 'name': 'Camber', 'version': ET.parse(ROOT/'Directory.Build.props').findtext('PropertyGroup/Version')},
                         'properties': [{'name': 'camber:review-status', 'value': 'Component inventory only. Native transitive provenance, dependency graph, full licenses and vulnerability review pending.'}]},
            'components': items}
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=ROOT / 'artifacts/beta/dependency-inventory.cdx.json')
args = parser.parse_args()
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(json.dumps(document, indent=2) + '\n',encoding='utf-8')
print(f'Inventoried {len(items)} components. Independent review remains required.')
