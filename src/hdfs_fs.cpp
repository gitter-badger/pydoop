// BEGIN_COPYRIGHT
// END_COPYRIGHT
#include <errno.h>
#include "hdfs_fs.hpp"


#define exec_and_trap_error(res_type_t, what,err_msg) \
  res_type_t res = what;                              \
  if (res < 0) {                                      \
    throw hdfs_exception(err_msg);                    \
  }


void wrap_hdfs_fs::disconnect() {
  exec_and_trap_error(int,
		      hdfsDisconnect(fs_),
		      "Cannot disconnect from " + host_);
}

tOffset wrap_hdfs_fs::get_default_block_size() {
  exec_and_trap_error(tOffset,
		      hdfsGetDefaultBlockSize(fs_),
		      "Cannot get default block_size of filesystem on" + host_);
  return res;
}

tOffset wrap_hdfs_fs::get_capacity() {
  exec_and_trap_error(tOffset,
		      hdfsGetCapacity(fs_),
		      "Cannot get capacity of filesystem on" + host_);
  return res;
}

tOffset wrap_hdfs_fs::get_used() {
  exec_and_trap_error(tOffset,
		      hdfsGetUsed(fs_),
		      "Cannot get amount of space used of filesystem on"
		        + host_);
  return res;
}

bool wrap_hdfs_fs::exists(const std::string& path) {
  int res = hdfsExists(fs_, path.c_str());
  return (res == 0);
}

void wrap_hdfs_fs::unlink(const std::string& path) {
  exec_and_trap_error(int,
		      hdfsDelete(fs_, path.c_str()),
		      "Cannot delete " + path + " in filesystem on " + host_);
}

void wrap_hdfs_fs::copy(const std::string& path, wrap_hdfs_fs& dst_fs,
			const std::string& dst_path) {
  exec_and_trap_error(int, 
		      hdfsCopy(fs_, path.c_str(), dst_fs.fs_, dst_path.c_str()),
		      "Cannot copy " + path + " to filesystem on "
		        + dst_fs.host_);
}

void wrap_hdfs_fs::move(const std::string& path, wrap_hdfs_fs& dst_fs,
			const std::string& dst_path) {
  exec_and_trap_error(int, 
		      hdfsMove(fs_, path.c_str(), dst_fs.fs_, dst_path.c_str()),
		      "Cannot move " + path + " to filesystem on " +
		        dst_fs.host_);
}

void wrap_hdfs_fs::rename(const std::string& old_path,
			  const std::string& new_path) {
  exec_and_trap_error(int, 
		      hdfsRename(fs_, old_path.c_str(), new_path.c_str()),
		      "Cannot rename " + old_path + " to " + new_path);
}

std::string wrap_hdfs_fs::get_working_directory() {
  std::size_t buff_size = 1024;
  char* buff = new char[buff_size];
  if (hdfsGetWorkingDirectory(fs_, buff, buff_size) == NULL) {
    throw hdfs_exception("Cannot get working directory");
  }
  std::string cwd(buff);
  delete [] buff;
  return cwd;
}

void wrap_hdfs_fs::set_working_directory(const std::string& path) {
  exec_and_trap_error(int, 
		      hdfsSetWorkingDirectory(fs_, path.c_str()),
		      "Cannot set working directory to " + path);
}

void wrap_hdfs_fs::create_directory(const std::string& path) {
  exec_and_trap_error(int, 
		      hdfsCreateDirectory(fs_, path.c_str()),
		      "Cannot create directory " + path);
}

void wrap_hdfs_fs::set_replication(const std::string& path, int replication){
  exec_and_trap_error(int, 
		      hdfsSetReplication(fs_, path.c_str(), replication),
		      "Cannot set replication of " + path);
}

static bp::dict list_directory_helper(hdfsFileInfo *info) {
  bp::dict d;
  if (info->mKind == kObjectKindFile) {
    d["kind"] = "file";
  } else if (info->mKind == kObjectKindDirectory) {
    d["kind"] = "directory";
  } else {
    d["kind"] = "unknown";
  }
  d["name"] = std::string(info->mName);
  d["owner"] = std::string(info->mOwner);
  d["group"] = std::string(info->mGroup);
  d["size"] = info->mSize;
  d["replication"] = info->mReplication;
  d["block_size"] = info->mBlockSize;
  d["permissions"] = info->mPermissions;
  d["last_access"] = info->mLastAccess;
  d["last_mod"]    = info->mLastMod;
  return d;
}

bp::list wrap_hdfs_fs::list_directory(std::string path) {
  hdfsFileInfo *res = 0;
  int num_entries = 0;
  bp::list l;
  // if path does not exist, hdfsListDirectory breaks before returning a value
  if (hdfsExists(fs_, path.c_str()) != 0) {
    throw hdfs_exception("No such file or directory: " + path);
  }
  res = hdfsListDirectory(fs_, path.c_str(), &num_entries);
  if (res == NULL && errno) {
    throw hdfs_exception("Cannot list directory " + path);
  }
  for(std::size_t i = 0; i < num_entries; ++i) {
    l.append(list_directory_helper(&(res[i])));
  }
  hdfsFreeFileInfo(res, num_entries);
  return l;
}

bp::dict wrap_hdfs_fs::get_path_info(std::string path) {
  hdfsFileInfo* res = hdfsGetPathInfo(fs_, path.c_str());
  if (res == NULL) {
    throw hdfs_exception("Cannot get path info for " + path);
  }
  bp::dict d = list_directory_helper(res);
  hdfsFreeFileInfo(res, 1);
  return d;
}

bp::list wrap_hdfs_fs::get_hosts(std::string path, tOffset start,
				 tOffset length) {
  char*** hosts = hdfsGetHosts(fs_, path.c_str(), start, length);
  if(hosts) {
    bp::list blocks;
    int i = 0;
    while(hosts[i]) {
      int j = 0;
      bp::list hosts_for_block;
      while(hosts[i][j]) {
	hosts_for_block.append(std::string(hosts[i][j]));
	++j;
      }
      blocks.append(hosts_for_block);
      ++i;
    }
    hdfsFreeHosts(hosts);
    return blocks;
  } else {
    throw hdfs_exception("Cannot get hosts per block for " + path);
  }
}

wrap_hdfs_file* wrap_hdfs_fs::open_file(std::string path, int flags, 
					int buffer_size, int replication, 
					int blocksize) {
  const char* c_path = (path.size() > 0) ? path.c_str() : NULL;
  hdfsFile f = hdfsOpenFile(fs_, c_path, flags, buffer_size,
			    replication, blocksize);
  if (f == NULL) {
    throw hdfs_exception("Cannot open file " + path +
			 " in filesystem on " + host_);
  }
  return new wrap_hdfs_file(path, this, f);
}


//++++++++++++++++++++++++++++++//
// Exporting class definitions. //
//++++++++++++++++++++++++++++++//

using namespace boost::python;

void export_hdfs_fs() {
  class_<wrap_hdfs_fs, boost::noncopyable>("hdfs_fs", init<std::string, int>())
    .def("close", &wrap_hdfs_fs::disconnect)
    .def("capacity",  &wrap_hdfs_fs::get_capacity)
    .def("default_block_size",  &wrap_hdfs_fs::get_default_block_size)
    .def("list_directory", &wrap_hdfs_fs::list_directory)
    .def("get_path_info",  &wrap_hdfs_fs::get_path_info)
    .def("get_hosts",  &wrap_hdfs_fs::get_hosts)
    .def("used",  &wrap_hdfs_fs::get_used)
    .def("exists",  &wrap_hdfs_fs::exists)
    .def("delete",  &wrap_hdfs_fs::unlink)
    .def("copy",    &wrap_hdfs_fs::copy)
    .def("move",    &wrap_hdfs_fs::move)
    .def("rename",    &wrap_hdfs_fs::rename)
    .def("working_directory",     &wrap_hdfs_fs::get_working_directory)
    .def("set_working_directory", &wrap_hdfs_fs::set_working_directory)
    .def("create_directory", &wrap_hdfs_fs::create_directory)
    .def("set_replication", &wrap_hdfs_fs::set_replication)
    .def("open_file", &wrap_hdfs_fs::open_file,
	 return_value_policy<manage_new_object>())
    ;
}
