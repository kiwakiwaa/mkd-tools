import os
import shutil
import subprocess

from support import DICTIONARIES, DictionaryTest


class CliTests(DictionaryTest):
    def run_cli(self, *arguments):
        return subprocess.run([os.environ['MKD_TEST_CLI'], '-d', str(DICTIONARIES), *arguments],
                              capture_output=True, text=True, encoding='utf-8')

    def test_lists_and_describes_the_small_dictionary(self):
        listing = self.run_cli('list')
        self.assertEqual(listing.returncode, 0, listing.stderr)
        self.assertIn('Birbs', listing.stdout)
        info = self.run_cli('info', 'Birbs')
        self.assertEqual(info.returncode, 0, info.stderr)
        self.assertIn('test.birbs', info.stdout)

    def test_exports_selected_entries_without_a_terminal(self):
        output = self.workspace() / 'export'
        result = self.run_cli('export', 'Birbs', '-o', str(output), '--only', 'entries', '--pretty')
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(len(list(output.rglob('*.xml'))), 3)
        self.assertFalse(list(output.rglob('*.png')))

    def test_help_and_invalid_invocations(self):
        self.assertEqual(self.run_cli('--help').returncode, 0)
        self.assertNotEqual(self.run_cli('export', 'MissingBirb').returncode, 0)
        self.assertNotEqual(self.run_cli('export', 'Birbs', '--only', 'lion-food').returncode, 0)

    def test_validates_a_package_without_a_dictionary_source(self):
        result = subprocess.run([os.environ['MKD_TEST_CLI'], 'validate', str(DICTIONARIES / 'Birbs')],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('0 errors', result.stdout)

    def test_validation_failure_status_survives_truncated_output(self):
        package = self.workspace() / 'Birbs'
        shutil.copytree(DICTIONARIES / 'Birbs', package)
        (package / 'Contents/orphan.html').write_text('<html><img src="missing.png"></html>')
        result = subprocess.run([os.environ['MKD_TEST_CLI'], 'validate', str(package),
                                 '--scan-all-documents', '--max-issues', '0'], capture_output=True, text=True)
        self.assertEqual(result.returncode, 1, result.stderr)
        self.assertIn('1 errors', result.stdout)

    def test_validation_distinguishes_invalid_packages_from_inaccessible_paths(self):
        package = self.workspace() / 'Malformed'
        package.mkdir()
        result = subprocess.run([os.environ['MKD_TEST_CLI'], 'validate', str(package)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 1)
        self.assertIn('[invalid-package]', result.stdout)
        result = subprocess.run([os.environ['MKD_TEST_CLI'], 'validate', str(package / 'absent')],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
