"""Create synthetic archives only inside a newly marked benchmark harness."""
import datetime
import json
from pathlib import Path
import struct
import sys
import uuid
import zlib

root = Path(sys.argv[1]).resolve()
count = int(sys.argv[2])
assert count in (0, 100, 514, 1000)
workflows = 85 if count == 514 else 5
archives = 42 if count == 514 else count
assert (root / 'AtlasLibraryBenchmark.fixture').is_file()
saved = root / 'Saved'
assert not (saved / 'Atlas').exists(), 'Never overwrite an existing dataset'
history = saved / 'Atlas' / 'History'
output = saved / 'Atlas' / 'Output'
history.mkdir(parents=True)
output.mkdir(parents=True)
(root / 'Content').mkdir(exist_ok=True)

def png(size):
    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))
    # Deterministic, compressible but non-uniform RGBA pixels; no third-party dependencies.
    row = b''.join(bytes((x % 256, (x // 8) % 256, 160, 255)) for x in range(size))
    data = zlib.compress((b'\0' + row) * size)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', size, size, 8, 6, 0, 0, 0)) + chunk(b'IDAT', data) + chunk(b'IEND', b'')

small, large = png(512), png(4096)
groups = {f'benchmark-{i}': [] for i in range(workflows)}
now = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0)
for index in range(count):
    workflow = index % workflows
    api = f'benchmark-{workflow}'
    folder = output / f'Workflow_{workflow}' / f'Run_{index:04d}'
    inputs, outputs = folder / 'Inputs', folder / 'Outputs'
    preview = outputs / 'preview.png'
    if index < archives:
        inputs.mkdir(parents=True)
        outputs.mkdir()
        preview.write_bytes(large if index < 5 else small)
    timestamp = (now - datetime.timedelta(minutes=index)).isoformat().replace('+00:00', 'Z')
    record = dict(jobId=str(uuid.uuid5(uuid.NAMESPACE_URL, f'atlas-library-benchmark/{index}')),
        apiId=api, workflowName=f'Benchmark Workflow {workflow}', startedAt=timestamp,
        completedAt=timestamp, durationSeconds=10, status='Success', errorMessage='',
        jobFolderPath=folder.as_posix(), inputsFolderPath=inputs.as_posix(), outputsFolderPath=outputs.as_posix(),
        jobJsonPath=(folder / 'job.json').as_posix(),
        inputs={'prompt': {'type':'String', 'value': 'Synthetic benchmark image; no service execution.'}},
        outputs={'image': {'type':'Image', 'filePath':preview.as_posix(), 'fileId':f'fixture-{index}'}},
        outputFilePaths={'image':preview.as_posix()})
    if index < archives:
        (folder / 'job.json').write_text(json.dumps(record), encoding='utf-8')
    groups[api].append(record)
for api, records in groups.items():
    if records:
        (history / (api + '.json')).write_text(json.dumps(records), encoding='utf-8')
(root / 'Dataset.json').write_text(json.dumps(dict(jobs=count, workflows=workflows if count else 0, perJobArchives=archives,
    largePreviews=min(count,5), largeDimensions=4096, otherDimensions=512,
    storage='All records in legacy arrays; per-job archives overlap the first records',
    cache='Generated immediately before launch; OS file cache is not flushed.'), indent=2), encoding='utf-8')
