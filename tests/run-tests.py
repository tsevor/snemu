import subprocess
import sys

def load_test(logpath):
	with open(logpath) as f:
		entries = f.read().split("\nTest")
		print("\n---\n".join(entries[:5]))

	return entries


tests = [
	("roms/cputest-basic.sfc", "logs/cputest-basic.txt"),
	# ("roms/cputest-full.sfc", "logs/cputest-full.txt"),
	# ("roms/spctest.sfc", "logs/spctest.txt"),
]

# update build
subprocess.run(["make"], check=True)

for rompath, logpath in tests:
	print(f"running test: {rompath}")
	log = load_test(logpath)
	with subprocess.Popen(["./snemu", rompath], stdout=subprocess.PIPE, text=True) as proc:
		for log_line, prog_line in zip(log, proc.stdout):
			if log_line != prog_line:
				print("mismatch")
				print(prog_line)
				print(log_line)
				break
