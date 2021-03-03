import sys
import subprocess
import pkg_resources

required = {'vosk', 'srt'}
installed = {pkg.key for pkg in pkg_resources.working_set}
missing = required - installed
print ("Missing pachages: ", missing)
if missing and len(sys.argv) > 1 :
    # install missing modules
    print ("Installing missing packages: ", missing)
    python = sys.executable
    subprocess.check_call([python, '-m', 'pip', 'install', *missing], stdout=subprocess.DEVNULL)
elif len(sys.argv) > 1 :
    if sys.argv[1] == 'upgrade':
        # update modules
        print ("Updating packages: ", required)
        python = sys.executable
        subprocess.check_call([python, '-m', 'pip', 'install', '--upgrade', *required], stdout=subprocess.DEVNULL)
    else:
        # check modules version
        python = sys.executable
        subprocess.check_call([python, '-m', 'pip', 'show', *required])
