#!/usr/bin/env python3
"""Reproduce Stage 0 diagnostics in an isolated directory; never edit HAVEN source."""
import argparse, pathlib, subprocess, tarfile, io, json, shutil, hashlib, difflib, time
P=pathlib.Path
HERE=P(__file__).resolve().parent
REV='f775304ce9443dc9861ba1880e4208c7ae19b314'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def command(argv,cwd,out,timeout=120):
 t=time.monotonic()
 with out.open('wb') as f:
  try:r=subprocess.run(argv,cwd=cwd,stdout=f,stderr=subprocess.STDOUT,timeout=timeout); code=r.returncode
  except subprocess.TimeoutExpired:code=124
 out.with_suffix(out.suffix+'.json').write_text(json.dumps({'argv':argv,'cwd':str(cwd),'timeout_s':timeout,'returncode':code,'elapsed_s':time.monotonic()-t},indent=2)+'\n')
 if code:raise RuntimeError(f'command failed ({code}): {out}')
def qemu(images,firmware,out,markers,direct=False):
 out.mkdir(parents=True,exist_ok=False)
 for n in ['haven.bin','guest_a.bin','guest_b.bin']:shutil.copy2(images/n,out/n)
 args=['qemu-system-aarch64','-machine','virt,'+('' if direct else 'secure=on,')+'virtualization=on,gic-version=3,iommu=smmuv3','-cpu','cortex-a72','-smp','4','-m','2G','-display','none','-monitor','none','-serial','file:uart.log']
 if not direct:
  for n in ['bl1.bin','bl2.bin','bl31.bin']:shutil.copy2(firmware/n,out/n)
  args+=['-bios','bl1.bin','-semihosting-config','enable=on,target=native']
 for n,addr in [('haven.bin','0x80000000'),('guest_a.bin','0x80800000'),('guest_b.bin','0xa0800000')]:
  args+=['-device',f'loader,file={n},addr={addr},force-raw=on'+(',cpu-num=0' if direct and n=='haven.bin' else '')]
 t=time.monotonic(); timed=False
 with (out/'console.log').open('wb') as f:
  proc=subprocess.Popen(args,cwd=out,stdout=f,stderr=subprocess.STDOUT)
  try:proc.wait(timeout=4)
  except subprocess.TimeoutExpired:
   timed=True;proc.terminate()
   try:proc.wait(timeout=2)
   except subprocess.TimeoutExpired:proc.kill();proc.wait()
 log=(out/'uart.log').read_text(errors='replace') if (out/'uart.log').exists() else ''
 result={'argv':args,'cwd':str(out),'timeout_s':4,'termination':'host timeout then SIGTERM' if timed else 'natural exit','returncode':proc.returncode,'elapsed_s':time.monotonic()-t,'required_markers':markers,'markers_present':all(m in log for m in markers),'image_sha256':{p.name:sha(p) for p in out.glob('*.bin')}}
 (out/'run.json').write_text(json.dumps(result,indent=2)+'\n')
 print(out.name,result['markers_present'],flush=True)
 print(log[-1800:],flush=True)
 return result
ap=argparse.ArgumentParser();ap.add_argument('--haven',type=P,required=True);ap.add_argument('--work',type=P,required=True);ap.add_argument('--replay-only',action='store_true');a=ap.parse_args()
a.haven=a.haven.resolve();a.work=a.work.resolve();a.work.mkdir(parents=True,exist_ok=True)
legacy=a.work/'legacy'
if not legacy.exists():
 with tarfile.open(HERE/'legacy-evidence.tar.gz') as t:t.extractall(legacy,filter='data')
firmware=legacy/'stage0-20261023-tfa-qemu-guest1'
if not (a.work/'replay-a').exists():qemu(firmware,firmware,a.work/'replay-a',['PARTITION_A: Post-fault check OK'])
if a.replay_only:raise SystemExit
src=a.work/'source'
if not src.exists():
 src.mkdir()
 with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(['git','-C',str(a.haven),'archive',REV]))) as t:t.extractall(src,filter='data')
 subprocess.run(['git','apply',str(HERE/'session-start-haven.patch')],cwd=src,check=True)
 p=src/'arch/arm64/boot.S';p.write_text(p.read_text().replace('        hvc     #0','        smc     #0'))
 p=src/'src/core/partition.c';p.write_text(p.read_text().replace('\t(void)cpu_id;','\t(void)cpu_id;\n\textern void stage0_prepare(void);\n\tstage0_prepare();'))
 p=src/'src/core/exc/el2_exceptions.c';p.write_text(p.read_text().replace('void hv_handle_sync(hv_u64 esr, hv_u64 far, hv_u64 sp)\n{','void hv_handle_sync(hv_u64 esr, hv_u64 far, hv_u64 sp)\n{\n if (((esr >> 26) & 63) == 0x16) { extern void stage0_report(uint64_t); stage0_report(sp); return; }'))
 p=src/'CMakeLists.txt';p.write_text(p.read_text().replace('        arch/arm64/cpu.c','        stage0_probe.c\n        arch/arm64/cpu.c'))
 shutil.copy2(HERE/'guest_probe.S',src/'tests/demos/guest_b_entry.S')
for mode,name in [(0,'virtual'),(1,'virtual-disabled'),(2,'physical-spi-scalar'),(3,'timer-detail'),(4,'timer-masked-detail'),(5,'timer-original-ppi-detail'),(6,'timer-future')]:
 if (a.work/name).exists():continue
 (src/'stage0_probe.c').write_text(f'#define PROBE_MODE {mode}\n'+(HERE/'probe.c').read_text())
 build=a.work/f'build-{name}';build.mkdir(exist_ok=True)
 command(['cmake','-S',str(src),'-B',str(build),'-G','Ninja','-DCMAKE_TOOLCHAIN_FILE='+str(src/'cmake/arm64.cmake'),'-DPLATFORM=qemu-virt','-DCMAKE_BUILD_TYPE=MinSizeRel'],a.work,build/'configure.log')
 command(['cmake','--build',str(build),'--parallel','4'],a.work,build/'build.log')
 qemu(build,firmware,a.work/name,[f'PROBE RESULT mode={mode}', ' PASS'])
 # Exact diagnostic source changes against pinned tracked base, including new C file.
 changes=[]
 for rel in ['CMakeLists.txt','arch/arm64/boot.S','src/core/partition.c','src/core/exc/el2_exceptions.c','tests/demos/guest_b_entry.S','stage0_probe.c']:
  old=subprocess.run(['git','-C',str(a.haven),'show',REV+':'+rel],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL).stdout.decode() if rel!='stage0_probe.c' else ''
  changes.extend(difflib.unified_diff(old.splitlines(True),(src/rel).read_text().splitlines(True),fromfile='a/'+rel if old else '/dev/null',tofile='b/'+rel))
 (a.work/name/'source.patch').write_text(''.join(changes))
 shutil.copy2(build/'haven.elf',a.work/name/'haven.elf')
 shutil.copy2(build/'guest_b.elf',a.work/name/'guest_b.elf')
