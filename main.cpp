#include <bitset>
#include <iostream>
#include <random>
#include <string>
#include <sstream>
#include <unordered_set>
#include <algorithm>
#include <memory>
#include <map>
using namespace std;


inline void cls()
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
	std::system("cls");
#else
	std::system("clear");
#endif
}

struct inode
{
	//string filename;
	size_t filesize;
	bool r, w, x;
	map<string, shared_ptr<inode>>* dir = nullptr;
	bool link = false;
	string data;
	inode() : filesize(0), r(true), w(true), x(true) {}
	string permissions() const
	{
		string res(4,0);
		res[0] = link ? 'l' : (dir ? 'd' : '-');
		res[1] = r ? 'r' : '-';
		res[2] = w ? 'w' : '-';
		res[3] = x ? 'x' : '-';
		return res;
	}
	bool isdir() const
	{
		return dir != nullptr;
	}
};

typedef shared_ptr<inode> inode_ptr;
typedef map<string,inode_ptr>* fcb_ptr;

random_device rd;
mt19937 gen(rd());


string random_string(const size_t length)
{
	constexpr char charset[] =
		"0123456789"
		//"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	constexpr size_t max_index = sizeof charset - 2;
	auto randchar = [&]() -> char
	{
		return charset[uniform_int_distribution<>(0, max_index)(gen)];
	};
	string str(length, 0);
	generate_n(str.begin(), length, randchar);
	return str;
}


class filesystem
{
	map<string, inode_ptr> MFD;
	inode_ptr root = nullptr;
	struct afd
	{
		unordered_set<inode*> open_files;
		fcb_ptr current_dir = nullptr;
		string username;
		string path = "/";
	}AFD;


	void parsecmdline(const string& cmdline)
	{
		string cmd;
		stringstream ss(cmdline);
		vector<string>params;
		while (ss >> cmd)
			params.push_back(cmd);
		if (params.empty())
			return;
		bool isFilterRole = true;

		if (params[0]=="sudo"){
			isFilterRole=false;
			params.erase(params.begin());
		}
		if (params.empty())
		{
			cout << "[!] Invalid Params" << endl;
			cout << "Usage: sudo [...]" << endl;
			return;
		}
			
		if (params[0] == "ls" || params[0] == "dir") {
			if (params.size() == 1)
				listuserfiles(false,false);
			else if (params[1].front()=='-'){
				bool detail = false;
				bool showinode = false;
				for(auto c:params[1]){
					if (c=='l')
						detail=true;
					else if (c=='i')
						showinode=true;
					else if (c!='-'){
						cout << "[!] Invalid Params" << endl;
						cout << "Usage: ls [-l][-i]" << endl;
						return;
					}
				}
				listuserfiles(detail,showinode);
			}
			else
			{
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: ls [-l][-i]" << endl;
			}

		}
		else if (params[0] == "open") {
			if (params.size() == 2) {
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (fd->isdir())
					cout << "[!] File [" << params[1] << "] is a directory" << endl;
				else if (AFD.open_files.find(fd.get()) != AFD.open_files.end())
					cout << "[!] File [" << params[1] << "] already opened" << endl;
				else
				{
					AFD.open_files.insert(fd.get());
					cout << "[+] File [" << params[1] << "] opened" << endl;
				}
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: open FILE" << endl;
			}
		}
		else if (params[0] == "close") {
			if (params.size() == 1) {
				const size_t num = AFD.open_files.size();
				AFD.open_files.clear();
				cout << "[+] " << num << " files closed" << endl;
			}
			else if (params.size() == 2) {
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (fd->isdir())
					cout << "[!] File [" << params[1] << "] is a directory" << endl;
				else if (AFD.open_files.find(fd.get()) == AFD.open_files.end())
					cout << "[!] File [" << params[1] << "] not opened" << endl;
				else
				{
					AFD.open_files.erase(fd.get());
					cout << "[+] File [" << params[1] << "] closed" << endl;
				}

			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: close [FILE]" << endl;
				cout << "If no FILE is specified, all open files are closed" << endl;
			}
		}
		else if (params[0] == "create") {
			if (params.size() == 2)
			{
				try
				{
					create(AFD.current_dir, params[1], 7);
					cout << "[+] File [" << params[1] << "] created" << endl;
				}
				catch (const std::exception& ex)
				{
					cout << "[!] " << ex.what() << endl;
				}
			}
			else if (params.size() == 3)
				try
				{
					create(AFD.current_dir, params[1], parseperm(params[2]));
					cout << "[+] File [" << params[1] << "] created" << endl;
				}
				catch (const std::exception& ex)
				{
					cout << "[!] " << ex.what() << endl;
				}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: create FILE [PERMISSIONS]" << endl;
			}
		}
		else if (params[0] == "delete" || params[0] == "rm" || params[0] == "del") {
			if (params.size() == 2)
			{
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (AFD.open_files.find(fd.get()) != AFD.open_files.end())
					cout << "[!] File [" << params[1] << "] is opened, please retry after closing it" << endl;
				else
				{
					try
					{
						deletefile(p.second.second, fd);
						cout << "[+] File [" << params[1] << "] deleted" << endl;
					}
					catch (const std::exception& ex)
					{
						cout << "[!] " << ex.what() << endl;
					}
				}
			}else if (params.size()==3){
				if (params[1] != "-rf")
				{
					cout << "[!] Invalid Params" << endl;
					cout << "Usage: delete [-rf] FILE/DIR" << endl;
					return;
				}
				const auto p = getfile(params[2],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[2] << "]: " << p.first << endl;
				else if (AFD.open_files.find(fd.get()) != AFD.open_files.end())
					cout << "[!] File [" << params[2] << "] is opened, please retry after closing it" << endl;
				else if (AFD.username.find(p.first) == 0 || p.first == AFD.username + "/")
					cout << "[!] You cannot delete folder [" << p.first << "] which contains your user folder " << endl;
				else
				{
					try
					{
						deletefile(p.second.second, fd, true);
						cout << "[+] File [" << params[2] << "] deleted" << endl;
					}
					catch (const std::exception& ex)
					{
						cout << "[!] " << ex.what() << endl;
					}
				}
				
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: delete [-rf] FILE/DIR" << endl;
			}
		}
		else if (params[0] == "read" || params[0] == "rd" || params[0] == "cat") {
			if (params.size() == 2)
			{
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (AFD.open_files.find(fd.get()) == AFD.open_files.end())
					cout << "[!] File [" << params[1] << "] is not opened, please retry after opening it" << endl;
				else
					read(fd);
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: read FILE" << endl;
			}

		}
		else if (params[0] == "write") {
			if (params.size() == 2)
			{
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (fd->isdir())
					cout << "[!] File [" << params[1] << "] is a directory" << endl;
				else if (AFD.open_files.find(fd.get()) == AFD.open_files.end())
					cout << "[!] File [" << params[1] << "] is not opened, please retry after opening it" << endl;
				else
				{
					cout << "[+] Please input data, Enter + Ctrl+Z to stop"<<endl;
					string data;
					string tmp;
					while(getline(cin,tmp))
						data += tmp + '\n';
					data.pop_back();	// remove last \n
					write(fd, data);
					cin.clear();

				}
			}else if (params.size() == 3)
			{
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (fd->isdir())
					cout << "[!] File [" << params[1] << "] is a directory" << endl;
				else if (AFD.open_files.find(fd.get()) == AFD.open_files.end())
					cout << "[!] File [" << params[1] << "] is not opened, please retry after opening it" << endl;
				else
					write(fd, params[2]);
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: write FILE [DATA]" << endl;
			}
		}
		else if (params[0] == "exec") {
			if (params.size() == 2)
			{
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (AFD.open_files.find(fd.get()) == AFD.open_files.end())
					cout << "[!] File [" << params[1] << "] is not opened, please retry after opening it" << endl;
				else
					run(fd);
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: exec FILE" << endl;
			}
		}
		else if (params[0] == "chmod") {
			if (params.size() == 3) {
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (AFD.open_files.find(fd.get()) != AFD.open_files.end())
					cout << "[!] File [" << params[1] << "] is opened, please retry after closing it" << endl;
				else
					try
					{
						chmod(fd, parseperm(params[2]));
						cout << "[+] Permission of file [" << params[1] << "] changed to " << fd->permissions() << endl;
					}
					catch (const std::exception& ex)
					{
						cout << "[!] " << ex.what() << endl;
					}
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: chmod FILE PERMISSIONS" << endl;
			}
		}
		else if (params[0] == "cd") {
			if (params.size() == 2) {
				if(params[1]=="/"){
					if(isFilterRole){
						cout << "[!] Permission denied" << endl;
						return;
					}
					AFD.current_dir = &MFD;
					AFD.path = "/";
					return;
				}
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (!fd->isdir())
					cout << "[!] File [" << params[1] << "] is not a directory" << endl;
				else
				{
					AFD.current_dir = fd->dir;
					AFD.path = p.first;
				}
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: cd DIR" << endl;
			}
		}
		else if (params[0] == "mkdir") {
			if (params.size() == 2)
			{
				try
				{
					create(AFD.current_dir, params[1], 7, true);
					cout << "[+] Dir [" << params[1] << "] created" << endl;
				}
				catch (const std::exception& ex)
				{
					cout << "[!] " << ex.what() << endl;
				}
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: mkdir DIR" << endl;
			}
		}
		else if (params[0] == "tree") {
			if (params.size() == 1)
			{
				cout << AFD.path << endl;
				tree(*AFD.current_dir);
			}
			else if (params.size() == 2)
			{
				if(params[1] == "/"){
					if(isFilterRole){
						cout<<"[!] Permission denied"<<endl;
						return;
					}
					cout << "/" << endl;
					tree(MFD);
					return;
				}
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else if (!fd->isdir())
					cout << "[!] File [" << params[1] << "] is not a directory" << endl;
				else
				{
					cout << p.first << endl;
					tree(*fd->dir);
				}
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: tree [DIR]" << endl;
			}
		}
		else if (params[0]=="ln"||params[0]=="mklink"){
			if(params.size()==3){
				const auto p = getfile(params[1],isFilterRole);
				const auto fd = p.second.first;
				if (!fd)
					cout << "[!] File [" << params[1] << "]: " << p.first << endl;
				else
				{
					try
					{
						mklink(AFD.current_dir, params[2], fd);
						cout << "[+] Link [" << params[2] << "] to "<< params[1] << " created" << endl;
					}
					catch (const std::exception& ex)
					{
						cout << "[!] " << ex.what() << endl;
					}
				}
			}
			else if (params.size() == 4)
			{
				if(params[1] != "-s"){
					cout << "[!] Invalid Params" << endl;
					cout << "Usage: ln [-s] source target" << endl;
					return;
				}
				try
				{
					auto file = create(AFD.current_dir, params[3], 7, false, true);
					file->data = params[2];
					cout << "[+] Symbolic Link [" << params[3] << "] to "<< params[2] << " created" << endl;
				}
				catch (const std::exception& ex)
				{
					cout << "[!] " << ex.what() << endl;
				}
			}
			else {
				cout << "[!] Invalid Params" << endl;
				cout << "Usage: ln [-s] source target" << endl;
			}
		}
		else if (params[0] == "pwd")
			cout << "[+] Current Path: " << AFD.path << endl;
		else if (params[0] == "whoami")
			cout << "[+] Current User: " << AFD.username << endl;
		else if (params[0] == "bye" || params[0] == "exit") {
			cout << "Good Bye :)" << endl;
			listuserfiles(true,true);
		}
		else if (params[0] == "logout") {
			cout << "[+] Logout" << endl;
			logout();
		}
		else if (params[0] == "cls" || params[0] == "clear")
			cls();
		else if (params[0] == "?" || params[0] == "help") {
			cout << "Available commands:" << endl;
			cout << "ls  open  close  create  delete  read  write  exec  chmod  mkdir  cd  tree  pwd  ln  whoami  exit  logout  cls  help" << endl;
		}
		else
			cout << "[!] Unknown Command" << endl;
	}

	static bitset<3> parseperm(const string& str)
	{
		// 0-7 
		if (str.size() == 1 && str[0] >= '0' && str[0] <= '7')
			return str[0] - '0';
		// 000-111
		bitset<3> ret;
		if (str.size() == 3 && all_of(str.begin(), str.end(), [](const char c) {return c == '0' || c == '1'; }))
		{
			for (size_t i = 0; i < 3; i++)
				ret[i] = str[i] - '0';
			return ret;
		}
		// r--
		if (str.size() == 3 && (str[0] == '-' || str[0] == 'r') && (str[1] == '-' || str[1] == 'w') && (str[2] == '-' || str[2] == 'x'))
		{
			ret[0] = str[0] == 'r';
			ret[1] = str[1] == 'w';
			ret[2] = str[2] == 'x';
			return ret;
		}
		// wx
		for (const auto c : str)
			switch (c)
			{
			case 'r':
				ret[0] = true;
				break;
			case 'w':
				ret[1] = true;
				break;
			case 'x':
				ret[2] = true;
				break;
			default:
				throw invalid_argument("Invalid Permission Identifier");
			}
		return ret;
	}

	pair<string, pair<inode_ptr,fcb_ptr>> parseabspath(const string& str){
		if (str.empty())
			return {"Invalid path",{nullptr,nullptr}};
		if (str[0] != '/')
			return {"Invalid path",{nullptr,nullptr}};
		auto path = str;
		if (path.back() == '/')
			path.pop_back();
		vector<string> ret;
		stringstream ss(path);
		string item;
		while (getline(ss, item, '/'))
			ret.push_back(item);
		if (ret.empty())
			return {"/",{root,&MFD}};
			
		auto current = &MFD;
		string simplepath = "/";
		for (size_t i = 1; i < ret.size(); i++)
		{
			
			auto fd = getfile(current, ret[i]);
			if (!fd)
				return {"Not found",{nullptr,nullptr}};
			if(ret[i]==".."){
				simplepath.pop_back();
				simplepath = simplepath.substr(0, simplepath.find_last_of('/'));
				simplepath.push_back('/');
			}else if(ret[i]!=".")
				simplepath += ret[i] + "/";

			if (i == ret.size() - 1){
				if(fd->link)
					return getfile(fd->data,false);
				return {simplepath,{fd,current}};
			}
				
			if (!fd->isdir())
				return {"Not Found",{nullptr,nullptr}};
			current = fd->dir;
		}
		return {"Not Found",{nullptr,nullptr}};
	}

	pair<string, pair<inode_ptr,fcb_ptr>> parserelativepath(const string& str){
		if (str.empty())
			return {"Invalid path",{nullptr,nullptr}};
		auto path = str;
		if (path.back() == '/')
			path.pop_back();
		vector<string> ret;
		stringstream ss(path);
		string item;
		while (getline(ss, item, '/'))
			ret.push_back(item);
		if (ret.empty())
			return {"Invalid path",{nullptr,nullptr}};
		auto current = AFD.current_dir;
		string simplepath = AFD.path;
		for (size_t i = 0; i < ret.size(); i++)
		{
			auto fd = getfile(current, ret[i]);
			if (!fd)
				return {"Not found",{nullptr,nullptr}};
			if(ret[i]==".."){
				simplepath.pop_back();
				simplepath = simplepath.substr(0, simplepath.find_last_of('/'));
				simplepath.push_back('/');
			}else if(ret[i]!=".")
				simplepath += ret[i] + "/";
			if (i == ret.size() - 1){
				if(fd->link)
					return getfile(fd->data,false);
				return {simplepath,{fd,current}};
			}
				
			if (!fd->isdir())
				return {"Not found",{nullptr,nullptr}};
			current = fd->dir;
		}
		return {"Not Found",{nullptr,nullptr}};
	}

	static string finduser(const string& username, const map<string, inode_ptr>& users, const string& path="/"){
		if (username=="..")
			return "";
		if(users.find(username)!=users.end())
			//return {path + username, users.at(username)};
			return path + username;
		for(const auto& p:users){
			if(p.first == "." || p.first == "..")
				continue;
			if(p.second->isdir()){
				auto ret = finduser(username, *p.second->dir, path + p.first + "/");
				if(!ret.empty())
					return ret;
			}
		}
		//return {"",nullptr};
		return "";
	}


	static bool isfilenamevalid(const string& str)
	{
		if (str.empty())
			return false;
		for (const auto c : str)
			if (c == '/' || c == '\\')
				return false;
		if(str == "." || str == "..")
			return false;
		return true;
	}

public:
	filesystem()
	{
		root = std::make_shared<inode>();
		root->dir = &MFD;
		MFD["."] = root;
	}

	void generatefiles()
	{
		for (int i = 1; i <= 5; i++)
		{
			string user = random_string(5);
			const auto usrdir = create(&MFD, user, 7, true);
			for (int j = 1; j <= 5; j++)
			{
				string filename = random_string(6);
				const auto f = std::make_shared<inode>();
				f->r = uniform_int_distribution<>(0, 1)(gen);
				f->w = uniform_int_distribution<>(0, 1)(gen);
				f->x = uniform_int_distribution<>(0, 1)(gen);
				f->filesize = uniform_int_distribution<>(1, 50)(gen);
				f->data = random_string(f->filesize);
				(*usrdir->dir)[filename] = f;
			}
			
		}
	}

	void generatetree(){
		const string level0 = "root";
		const vector<string> level1 = {"dept1","dept2","dept3","a"};
		map<string,vector<string>> level2 = {
			{"dept1",{"u1","user2","user3"}},
			{"dept2",{"user4","user5","user6"}},
			{"dept3",{"user7","user8","user9","user10"}},
			{"a",{"bb","cc","dd","ee"}}
		};
		const auto dir = create(&MFD, level0, 7, true);
		auto genrandfile = [&](const inode_ptr& dr,const int num=2){
			for(int i=0;i<num;i++){
				string filename = random_string(5);
				const auto f = std::make_shared<inode>();
				//f->filename = filename;
				f->r = uniform_int_distribution<>(0, 1)(gen);
				f->w = uniform_int_distribution<>(0, 1)(gen);
				f->x = uniform_int_distribution<>(0, 1)(gen);
				f->filesize = uniform_int_distribution<>(1, 50)(gen);
				f->data = random_string(f->filesize);
				(*dr->dir)[filename] = f;
			}
		};
		for(auto& d:level1){
			const auto d1 = create(dir->dir, d, 7, true);
			for(auto& d2:level2[d]){
				const auto d3 = create(d1->dir, d2, 7, true);
				genrandfile(d3);
			}
			genrandfile(d1);
		}


	}

	static inode_ptr getfile(fcb_ptr usermap, const string& filename)
	{
		if (usermap->find(filename) == usermap->end())
			return nullptr;
		return (*usermap)[filename];
	}

	pair<string,pair<inode_ptr,fcb_ptr>> getfile(const string& path,bool filterrole=true)
	{
		pair<string,pair<inode_ptr,fcb_ptr>> ret;
		if (path.empty())
			throw invalid_argument("Empty Path");
		if (path[0] == '/')
			ret = parseabspath(path);
		else
			ret = parserelativepath(path);
		if(!ret.second.first)		// Already Encountered Error
			return ret;
		if(filterrole && ret.first.find(AFD.username) != 0)
			return {"Permission Denied",{nullptr,nullptr}};
		return ret;
	}

	static inode_ptr create(fcb_ptr usermap, const string& filename, bitset<3> perm, const bool isdir=false, const bool islink=false)
	{
		if (usermap->find(filename) != usermap->end())
			throw runtime_error("File [" + filename + "] already exists");
		if (!isfilenamevalid(filename))
			throw runtime_error("Invalid Filename [" + filename + "]");
		if (islink && isdir)
			throw runtime_error("Invalid params");
		auto f = std::make_shared<inode>();
		//f->filename = filename;
		f->r = perm[0];
		f->w = perm[1];
		f->x = perm[2];
		(*usermap)[filename] = f;
		if (isdir){
			f->dir = new map<string, inode_ptr>();
			(*f->dir)["."] = f;
			(*f->dir)[".."] = (*usermap)["."];
		}
		f->link = islink;

		return f;
	}

	static inode_ptr mklink(fcb_ptr usermap, const string& filename, const inode_ptr& src)
	{
		if (usermap->find(filename) != usermap->end())
			throw runtime_error("File [" + filename + "] already exists");
		if (!isfilenamevalid(filename))
			throw runtime_error("Invalid Filename [" + filename + "]");
		if (!src)
			throw runtime_error("Invalid Target");
		if (src->isdir())
			throw runtime_error("Cannot create link to directory");
		(*usermap)[filename] = src;
		return src;
	}



	static void deletefile(fcb_ptr usermap, const inode_ptr& file, bool recursive=false){
		if (!file)
			return;
		// refuse to delete . and .. directory
		if((*usermap)["."].get() == file.get() || (*usermap)[".."].get() == file.get())
			throw runtime_error("Cannot delete '.' or '..' directory");
		if (file->isdir() && recursive)
		{
			while(!file->dir->empty()){
				const auto it = file->dir->begin();
				if (it->first == ".." || it->first == "."){
					file->dir->erase(it);
					//it->second already deleted by erase;
					continue;
				}
				deletefile(file->dir, it->second, recursive);
			}
		}
		else if(file->isdir() && !recursive)
			throw runtime_error("Directory is not empty");

		// delete file in user map
		for(auto it=usermap->begin();it!=usermap->end();++it){
			if(it->second.get() == file.get()){
				usermap->erase(it);
				break;
			}
		}
	
		// if (usermap->find(file->filename) == usermap->end())
		// 	throw runtime_error("File [" + file->filename + "] not found");
		// usermap->erase(file->filename);
	}

	static void chmod(const inode_ptr& f, const bool r, const bool w, const bool x)
	{
		f->r = r;
		f->w = w;
		f->x = x;
	}

	static void chmod(const inode_ptr& f, bitset<3> bits)
	{
		f->r = bits[0];
		f->w = bits[1];
		f->x = bits[2];
	}

	static void read(const inode_ptr& f)
	{
		if(f->isdir()){
			cout << "[!] "<< f.get() <<" is a directory" << endl;
			return;
		}
		if (f->r)
			cout << "Data of [" << f.get() << "]" << endl << f->data << endl;
		else
			cout << "[!] Permission denied" << endl;
	}

	static void write(const inode_ptr& f, const string& data)
	{
		if(f->isdir()){
			cout << "[!] "<< f.get() <<" is a directory" << endl;
			return;
		}
		if (f->w)
		{
			f->data = data;
			f->filesize = data.size();
			cout << "[" << f.get() << "] Written" << endl;
		}
		else
			cout << "[!] Permission denied" << endl;
	}

	static void run(const inode_ptr& f)
	{
		if(f->isdir()){
			cout << "[!] "<< f.get() <<" is a directory" << endl;
			return;
		}
		if (f->x)
			cout << "Running [" << f.get() << "]" << endl;
		else
			cout << "[!] Permission denied" << endl;
	}

	static void tree(const map<string,inode_ptr>& root, const string& prefix = "")
	{
		for(auto it = root.begin(); it != root.end(); ++it){
			if(it->first == ".." || it->first == ".")
				continue;
			if(next(it) == root.end()){
				if(it->second->isdir()){
					cout << prefix << "└── " << it->first << endl;
					tree(*it->second->dir, prefix + "    ");
				}
				else
					cout << prefix << "└── " << it->first << endl;
			}
			else{
				if(it->second->isdir()){
					cout << prefix << "├── " << it->first << endl;
					tree(*it->second->dir, prefix + "│   ");
				}
				else
					cout << prefix << "├── " << it->first << endl;
			}
		}
	}

	void listuserfiles(const bool detailed, const bool showinode, const string& path = "", const fcb_ptr dirmap = nullptr) const
	{
		string p = path;
		if(path.empty())
			p = AFD.path;
		auto dir = dirmap;
		if(!dirmap)
			dir = AFD.current_dir;
		
		if (detailed)
		{
			cout << "Index of " << p << endl;
			cout << "Total:" << dir->size() << endl;
			if(showinode)
				cout << "Inode\tPerm\tRef\tSize\tFilename" << endl;
			else
				cout << "Perm\tref\tSize\tFilename" << endl;
			for (const auto& file : *dir){
				if(showinode)
					cout << file.second.get() << "\t";
				cout << file.second->permissions()<< "\t" << file.second.use_count() << "\t" << file.second->filesize << "\t" << file.first;
				if(file.second->link)
					cout << " -> " << file.second->data;
				cout << endl;
			}
				
		}
		else
		{
			for (const auto& file : *dir){
				if(showinode)
					cout << file.second.get() << "  " << file.first << "  ";
				else
					cout << file.first << "  ";
			}
				
			cout << endl;
		}
	}

	void logout()
	{
		AFD.username = "";
		AFD.current_dir = nullptr;
		AFD.open_files.clear();
	}

	[[noreturn]] void cmdloop()
	{
		string user, line;
		while (true)
		{
			while (!AFD.username.empty() && AFD.current_dir)
			{
				getline(cin, line);
				parsecmdline(line);
				if(!AFD.username.empty())cout << "[" << AFD.username << " " << AFD.path <<"]$ ";
			}
			tree(MFD);
			cout << "Login: ";
			cin >> user;
			// if (MFD.find(user) != MFD.end() && MFD[user]->isdir())
			// {
			// 	AFD.path += user + "/";
			// 	AFD.current_dir = MFD[user]->dir;
			// 	AFD.username = user;
			// 	continue;
			// }
			auto res = finduser(user,MFD);
			if(!res.empty()){
				AFD.path = res + "/";
				AFD.current_dir = parseabspath(res).second.first->dir;
				AFD.username = res;
				continue;
			}
			cout << "[!] User not found" << endl;
			
		}
	}
};


int main()
{

	filesystem fs;
	//fs.generatefiles();
	fs.generatetree();
	fs.cmdloop();
}


