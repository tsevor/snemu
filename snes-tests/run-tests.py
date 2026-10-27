import subprocess
import sys

def load_test(logpath):
	with open(logpath) as f:
		pass



tests = [
	("roms/cputest-basic.sfc", "logs/cputest-basic.txt"),
	# ("roms/cputest-full.sfc", "logs/cputest-full.txt"),
	# ("roms/spctest.sfc", "logs/spctest.txt"),
]



for rompath, logpath in tests:
	print(f"running test: {rompath}")
	log = load_test(logpath)
