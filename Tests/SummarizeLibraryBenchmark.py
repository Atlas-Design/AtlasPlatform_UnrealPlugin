"""Summarize successful isolated captures; retain run IDs for reproducibility."""
import csv
import json
from pathlib import Path
import statistics
import sys

root = Path(sys.argv[1])
destination = Path(sys.argv[2])
destination.mkdir(parents=True, exist_ok=True)
rows = []
allowed_runs = set(sys.argv[3:])
for run in sorted(root.iterdir()):
    if allowed_runs and run.name not in allowed_runs:
        continue
    report_path = run / 'Report/index.json'
    if not report_path.exists():
        continue
    report = json.loads(report_path.read_text(encoding='utf-8-sig'))
    if report.get('failed') or report.get('succeeded', 0) + report.get('succeededWithWarnings', 0) != 1:
        continue
    dataset = json.loads((run / 'Dataset.json').read_text(encoding='utf-8-sig'))
    for name in ('FirstOpen', 'WarmOpen1', 'WarmOpen2'):
        path = run / 'Saved/AtlasProfiling' / (name + '.json')
        capture = json.loads(path.read_text(encoding='utf-8-sig'))
        metrics, events = capture['measurements'], capture['events']
        def event(key, field='calls'):
            return events.get(key, {}).get(field, 0)
        rows.append(dict(run=run.name, capture=name, jobs=metrics['jobs'], workflows=dataset['workflows'],
            openMs=metrics['openCallMs'], assetLoadMs=event('Panel.LoadAsset', 'totalMs'),
            spawnMs=event('Panel.SpawnTab', 'totalMs'), scanMs=event('History.ScanArchives', 'totalMs'),
            scans=event('History.ScanArchives'), jobReads=event('History.ReadJobJson'),
            recordParses=event('History.ParseRecord'), workflowInfoCalls=event('History.WorkflowInfo'),
            historyQueries=event('History.Query'), previewLoads=event('Preview.LoadImage') + event('Preview.LoadTexture'),
            workflowDiscoveryMs=event('Workflow.DiscoverUI', 'totalMs') + event('Workflow.DiscoverManager', 'totalMs'),
            widgetDelta=metrics['widgetObjectsAfter'] - metrics['widgetObjectsBefore'],
            textureDelta=metrics['textureObjectsAfter'] - metrics['textureObjectsBefore'],
            maxTickGapMs=metrics['maxObservedTickGapMs'],
            observedMemoryGrowthMiB=(metrics['peakObservedPhysicalBytes'] - metrics['physicalBytesBefore']) / 1048576,
            processPeakMiB=metrics['processPeakPhysicalBytes'] / 1048576))
assert rows, 'No successful captures found'
with (destination / 'LibraryBaseline.csv').open('w', newline='', encoding='utf-8') as stream:
    writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
    writer.writeheader()
    writer.writerows(rows)
(destination / 'LibraryBaseline.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
lines = ['| Jobs | Workflows | Opening | Samples | Median ms | Min-max ms | Scans | Job reads | Preview loads |',
         '|---|---|---|---|---|---|---|---|---|']
for jobs in sorted({row['jobs'] for row in rows}):
    for kind in ('First', 'Warm'):
        group = [row for row in rows if row['jobs'] == jobs and row['capture'].startswith(kind)]
        times = [row['openMs'] for row in group]
        lines.append(f"| {jobs} | {group[0]['workflows']} | {kind} | {len(group)} | {statistics.median(times):.1f} | {min(times):.1f}-{max(times):.1f} | {group[0]['scans']} | {group[0]['jobReads']} | {group[0]['previewLoads']} |")
text = '\n'.join(lines) + '\n'
(destination / 'LibraryBaselineTable.md').write_text(text, encoding='utf-8')
print(text)
