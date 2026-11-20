"""Execute the worked namespace.md example verbatim and check its final state."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--probe', required=True)
parser.add_argument('--concept', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
section = args.concept.read_text(encoding='utf-8').split('## 11.', 1)[1].split('## 12.', 1)[0]
example = re.search(r'```cypher\n(.*?)```', section, re.S).group(1)
with tempfile.TemporaryDirectory(prefix='riag_namespace_example_') as temporary:
    directory = Path(temporary)
    database = directory / 'database'
    script = directory / 'example.cypher'
    script.write_text(example)
    result = subprocess.run([args.probe, str(database), 'script', str(script)],
                            text=True, capture_output=True, check=False)
    assert result.returncode == 0, result.stderr
    checks = []
    for query, expected in [
            ('MATCH (p:Person) RETURN count(*)', '3'),
            ('MATCH (p:Person IN team_b) RETURN count(*)', '2'),
            ('MATCH ()-[r:Knows]->() RETURN count(*)', '0'),
            ('MATCH (a:Asset) RETURN count(*)', '0'),
            ('CALL show_namespaces() RETURN count(*)', '3')]:
        actual = subprocess.run([args.probe, str(database), 'query', query],
                                text=True, capture_output=True, check=True).stdout.strip()
        assert actual == expected, (query, actual, expected)
        checks.append({'query': query, 'result': actual})
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({'example_executed': True, 'checks': checks,
                                     'example_output': result.stdout}, indent=2))
print('Worked example executed verbatim; all five final-state checks passed.')
