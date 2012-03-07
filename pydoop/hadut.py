
# BEGIN_COPYRIGHT
# END_COPYRIGHT

import copy
import os
import subprocess

def __is_exe(fpath):
	return os.path.exists(fpath) and os.access(fpath, os.X_OK)

def num_nodes():
	"""
	Get the number of task tracker in the cluster.
	"""
	hproc = subprocess.Popen([hadoop, "job", "-list-active-trackers"], stdout=subprocess.PIPE)
	stdout, stderr = hproc.communicate()
	if hproc.returncode == 0:
		return stdout.count("\n") # trackers are returned one per line
	else:
		raise RuntimeError("Error running hadoop job -list-active-trackers")

def hdfs_path_exists(path):
	"""
	stat the given HDFS path.
	"""
	retcode = subprocess.call([hadoop, 'dfs', '-stat', path], stdout=open('/dev/null', 'w'), stderr=subprocess.STDOUT)
	return retcode == 0

def run_hadoop_cmd_e(cmd, properties=None, args_list=[]):
	"""
	Run a Hadoop command.  Launches an exception in case of failure.
	"""
	retcode = run_hadoop_cmd(cmd, properties, args_list)
	if retcode != 0:
		raise RuntimeError("Error running Hadoop command")

def run_hadoop_cmd(cmd, properties=None, args_list=[]):
	"""
	Run a Hadoop command.  Returns the exit code.
	"""
	args = [hadoop, cmd]
	if properties:
		args += __construct_property_args(properties)
	args += map(str, args_list) # only string arguments are allowed
	return subprocess.call(args)

def dfs(*args):
	"""
	Run the Hadoop dfs command.  Given the specific command (e.g. -ls)
	in the method arguments.
	
	Launches an exception in case of failure.
	"""
	return run_hadoop_cmd_e("dfs", args_list=args)

def run_hadoop_jar(jar_name, properties=None, args_list=[]):
	"""
	Run a jar on Hadoop (hadoop jar command).
	Launches an exception in case of failure.
	"""
	if os.path.exists(jar_name) and os.access(jar_name, os.R_OK):
		args = [hadoop, 'jar', jar_name]
		if properties:
			args += __construct_property_args(properties)
		args += map(str, args_list)
		retcode = subprocess.call(args)
		if retcode != 0:
			raise RuntimeError("Error running hadoop jar")
	else:
		raise ValueError("Can't read jar file %s" % jar_name)

def __construct_property_args(prop_dict):
	return sum(map(lambda pair: ["-D", "%s=%s" % pair], prop_dict.iteritems()), []) # sum flattens the list

def run_pipes(executable, input_path, output_path, properties=None, args_list=[]):
	"""
	Run a pipes command.  Returns exit code.
	"""
	args = [hadoop, "pipes"]
	properties = properties.copy() if properties else {}
	properties['hadoop.pipes.executable'] = executable

	args.extend( __construct_property_args(properties) )
	args.extend(args_list)
	args.extend(["-input", input_path, "-output", output_path])
	return subprocess.call(args)

def run_class(class_name, additional_cp=None, properties=None, args_list=[]):
	"""
	Run a class that needs the Hadoop jars in its class path.
	Returns the exit code.
	"""
	args = [hadoop, class_name]
	if additional_cp:
		env = copy.copy(os.environ)
		if type(additional_cp) == str: # wrap a single class path in a list
			additional_cp = [additional_cp]
		env['HADOOP_CLASSPATH'] = ":".join(additional_cp)
	else:
		env = os.environ
	if properties:
		args.extend( __construct_property_args(properties) )
	args.extend(args_list)
	return subprocess.call(args)

def run_class_e(class_name, additional_cp=None, properties=None, args_list=[]):
	"""
	Run a class that needs the Hadoop jars in its class path
	Launches an exception in case of failure.
	"""
	retcode = run_class(class_name, additional_cp, properties, args_list)
	if retcode != 0:
		raise RuntimeError("Error running Hadoop class")


def find_jar(jar_name, root_path=None):
	"""
	Look for the named jar in:

	* root_path, if specfied;
	* current-working directory (cwd);
	* cwd/build;
	* /usr/share/java

	Returns the full path of the jar if found; else returns None.
	"""
	root = root_path or os.getcwd()
	paths = (root, os.path.join(root, "build"), "/usr/share/java")
	for path in [ os.path.join(path, jar_name) for path in paths ]:
		if os.path.exists(path):
			return path
	return None

#################################################################################
# module initialization
#################################################################################

hadoop = None
"""
The path to the ``hadoop`` executable found.

pydoop.hadut searches in ``HADOOP_HOME/bin``, then scans the ``PATH``.

An ImportError is raised if the ``hadoop`` executable isn't found.
"""

if os.environ.has_key("HADOOP_HOME") and \
	__is_exe(os.path.join(os.environ["HADOOP_HOME"], "bin", "hadoop")):
	hadoop = os.path.join(os.environ["HADOOP_HOME"], "bin", "hadoop")
else:
	# search the PATH for hadoop
	for path in os.environ["PATH"].split(os.pathsep):
		hpath = os.path.join(path, 'hadoop')
		if __is_exe(hpath):
			hadoop = hpath
			break
	if hadoop is None:
		raise ImportError("Couldn't find hadoop executable.  Please set HADOOP_HOME or add the hadoop executable to your PATH")
	hadoop = os.path.abspath(hadoop)