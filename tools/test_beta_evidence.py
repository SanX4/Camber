import copy
import hashlib
import tempfile
import unittest
from pathlib import Path
from beta_evidence import REQUIRED, ARTIFACTS, evaluate


class EvidenceGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        file = self.root / 'report'
        file.write_bytes(b'candidate evidence')
        record = {'path': 'report', 'sha256': hashlib.sha256(file.read_bytes()).hexdigest()}
        hashes = {name: record['sha256'] for name in ARTIFACTS}
        self.evidence = {'sourceCommit': 'abc', 'artifacts': {name: copy.deepcopy(record) for name in ARTIFACTS},
                         'checks': {name: dict(record, status='pass', sourceCommit='abc', candidateHashes=hashes) for name in REQUIRED}}

    def test_matching_evidence_passes(self):
        self.assertEqual([], evaluate(self.root, self.evidence, 'abc', False))

    def test_missing_evidence_fails(self):
        self.assertTrue(evaluate(self.root, {}, None, True))

    def test_changed_file_fails(self):
        (self.root / 'report').write_bytes(b'changed after approval')
        self.assertTrue(evaluate(self.root, self.evidence, 'abc', False))

    def test_dirty_or_changed_commit_fails(self):
        self.assertTrue(evaluate(self.root, self.evidence, 'abc', True))
        self.assertTrue(evaluate(self.root, self.evidence, 'def', False))

    def test_outside_project_path_fails(self):
        self.evidence['checks']['privacy']['path'] = '../report'
        self.assertTrue(evaluate(self.root, self.evidence, 'abc', False))

    def test_old_artifact_approval_fails(self):
        self.evidence['checks']['ownerApproval']['candidateHashes'] = {'windowsInstaller': 'old'}
        self.assertTrue(evaluate(self.root, self.evidence, 'abc', False))

    def test_missing_platform_candidate_fails(self):
        del self.evidence['artifacts']['androidBundle']
        self.assertTrue(evaluate(self.root, self.evidence, 'abc', False))


if __name__ == '__main__':
    unittest.main()
