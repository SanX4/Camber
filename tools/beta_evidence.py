"""Validation of recorded release evidence; no deployment side effects."""
import hashlib

REQUIRED = ('source', 'runtime', 'android16k', 'recognition', 'ui', 'privacy',
            'crossUserIsolation', 'independentSecurityReview', 'videoPerformance',
            'endurance', 'compatibility', 'installer', 'windowsSigning',
            'androidSigning', 'sbom', 'distributionSetup', 'ownerApproval')
ARTIFACTS = ('windowsInstaller', 'androidBundle')


def verify_file(root, record):
    path = (root / record['path']).resolve()
    path.relative_to(root.resolve())
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != record['sha256']:
        raise ValueError('missing or changed file')


def evaluate(root, evidence, head, dirty):
    blocked = []
    if not head:
        blocked.append('source: no committed baseline')
    if dirty:
        blocked.append('source: uncommitted or untracked changes')
    if not head or evidence.get('sourceCommit') != head:
        blocked.append('source: evidence belongs to another commit')
    artifacts = evidence.get('artifacts', {})
    hashes = {}
    for name in ARTIFACTS:
        try:
            verify_file(root, artifacts[name])
            hashes[name] = artifacts[name]['sha256']
        except (OSError, KeyError, TypeError, ValueError):
            blocked.append(name + ': missing or changed candidate')
    for key in REQUIRED:
        check = evidence.get('checks', {}).get(key, {})
        if check.get('status') != 'pass':
            blocked.append(key)
            continue
        try:
            verify_file(root, check)
            if check.get('sourceCommit') != head or len(hashes) != len(ARTIFACTS) or check.get('candidateHashes') != hashes:
                raise ValueError('wrong candidate')
        except (OSError, KeyError, TypeError, ValueError):
            blocked.append(key + ': missing, changed or obsolete evidence')
    return blocked
