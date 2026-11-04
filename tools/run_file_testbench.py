#!/usr/bin/env python3
"""Generic netlist + testbench files to trace files, using the existing C++ engine."""
import argparse
from pathlib import Path
import re
import subprocess
import tempfile
import shutil
import sys

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description='Run any supported netlist with a .tb stimulus/assertion file')
p.add_argument('--netlist',required=True,type=Path,help='.table/.tbl or FSM .net file')
p.add_argument('--testbench',required=True,type=Path,help='.tb file containing stimuli and assertions')
p.add_argument('--out-dir',type=Path,default=Path('file_run_output'))
a=p.parse_args()
try:
    source=a.netlist.resolve(strict=True)
    tb=a.testbench.resolve(strict=True)
    if source.suffix.lower() not in ('.table','.tbl','.net') or tb.suffix.lower()!='.tb':
        raise ValueError('Use a .table/.tbl/.net netlist and .tb testbench file.')
    kind='FSM' if source.suffix.lower()=='.net' else 'NETLIST'
    lines=tb.read_text(encoding='utf-8').splitlines()
    (ROOT/'build').mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='cli_file_',dir=ROOT/'build') as d:
        copied=Path(d)/('uploaded'+source.suffix.lower())
        shutil.copyfile(source,copied)
        net_ref=str(copied.relative_to(ROOT))
        for i,line in enumerate(lines):
            if re.match(r'^\s*(?:NETLIST|FSM)\s+',line,re.I):
                lines[i]=kind+' '+net_ref
                break
        else:
            lines.insert(0,kind+' '+net_ref)
        path=Path(d)/'input.tb';path.write_text('\n'.join(lines)+'\n',encoding='utf-8')
        r=subprocess.run([str(ROOT/'simulator'),'--testbench',str(path)],cwd=ROOT)
    if r.returncode:
        raise RuntimeError(f'Simulator exited with status {r.returncode}')
    out=a.out_dir.resolve();out.mkdir(parents=True,exist_ok=True)
    for filename in ('simulation.vcd','waveform.txt','waveform.json','results.txt'):
        src=ROOT/'output'/filename
        if not src.exists():raise FileNotFoundError('Missing generated output: '+filename)
        shutil.copy2(src,out/filename)
    print('Output files saved to:',out)
except (ValueError,OSError,RuntimeError) as exc:
    print('ERROR:',exc,file=sys.stderr)
    sys.exit(1)
