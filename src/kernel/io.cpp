#include "io.h"
#include "kernel.h"
#include "Handler.h"
#include "PipeHandler.h"
#include "Pipe.h"

#include <memory>


void HandleIO(kiv_os::TRegisters &regs) {
	
	switch (regs.rax.l) {
		case kiv_os::scCreate_File: {
			create_file(regs);
			break;
		}

		case kiv_os::scWrite_File: {
			write_file(regs);
			break;
		}

		case kiv_os::scRead_File: {
			read_file(regs);
			break;
		}

		case kiv_os::scDelete_File: {
			delete_file(regs);
			break;
		}

		case kiv_os::scSet_File_Position: {
			set_file_position(regs);
			break;
		}

		case kiv_os::scGet_File_Position: {
			get_file_position(regs);
			break;
		}

		case kiv_os::scClose_Handle: {
			close_handle(regs);
			break;
		}

		case kiv_os::scGet_Current_Directory: {
			get_current_directory(regs);
			break;
		}

		case kiv_os::scSet_Current_Directory: {
			set_current_directory(regs);
			break;
		}
			
		case kiv_os::scCreate_Pipe: {
			create_pipe(regs);
			break;
		}
	}
}

void create_file(kiv_os::TRegisters &regs) {
	
	std::string path(reinterpret_cast<char*>(regs.rdx.r));
	uint8_t open_always = static_cast<uint8_t>(regs.rcx.r);
	uint8_t file_atributes = static_cast<uint8_t>(regs.rdi.r);

	FileHandler * handler = NULL;
	uint16_t ret_code = 0;

	if (path.find(":") == std::string::npos) {
		path = processManager->get_proc_work_dir() + DELIMETER + path;
	}

	path_compiler(path);
	
	if (file_atributes & kiv_os::faDirectory) { 
		if (open_always == kiv_os::fmOpen_Always) {
			// open dir
			ret_code = vfs->open_object(&handler, path, FS::FS_OBJECT_DIRECTORY);
		}
		else {
			// crate dir
			ret_code = vfs->create_dir(&handler, path);
		}
	}
	else {
		if (open_always == kiv_os::fmOpen_Always) {
			// open file
			ret_code = vfs->open_object(&handler, path, FS::FS_OBJECT_FILE);
		}
		else {
			// crate file
			ret_code = vfs->create_file(&handler, path);
		}
	}

	if (ret_code != kiv_os::erSuccess) {
		set_error(regs, ret_code);
		return;
	}

	std::shared_ptr<Handler> shared_handler(handler);
	kiv_os::THandle t_handle = processManager->add_handle(shared_handler);

	regs.rax.x = static_cast<decltype(regs.rax.x)>(t_handle);
}

void write_file(kiv_os::TRegisters &regs) {
	size_t written;

	std::shared_ptr<Handler> handle = processManager->get_handle_object(static_cast<kiv_os::THandle>(regs.rdx.x));
	if (!handle) {
		set_error(regs, kiv_os::erInvalid_Handle);
		return;
	}

	uint16_t ret_code = handle->write(reinterpret_cast<char*>(regs.rdi.r), static_cast<size_t>(regs.rcx.r), written);
	if (ret_code != kiv_os::erSuccess) {
		set_error(regs, ret_code);
		return;
	}
	
	regs.rax.r = static_cast<decltype(regs.rax.r)>(written);
}

void read_file(kiv_os::TRegisters &regs) {
	size_t read;
	std::shared_ptr<Handler> handle = processManager->get_handle_object(static_cast<kiv_os::THandle>(regs.rdx.x));
	if (!handle) {
		set_error(regs, kiv_os::erInvalid_Handle);
		return;
	}
	
	uint16_t ret_code = handle->read(reinterpret_cast<char*>(regs.rdi.r), static_cast<size_t>(regs.rcx.r), read);
	if (ret_code != kiv_os::erSuccess) {
		set_error(regs, ret_code);
		return;
	}

	regs.rax.r = static_cast<decltype(regs.rax.r)>(read);
}

void delete_file(kiv_os::TRegisters &regs)
{
	std::string path(reinterpret_cast<char*>(regs.rdx.r));

	if (path.find(":") == std::string::npos) {
		path = processManager->get_proc_work_dir() + DELIMETER + path;
	}
	path_compiler(path);

	uint16_t ret_code = vfs->remove_file(path); // remove file

	if (ret_code == kiv_os::erFile_Not_Found) {

		if (!processManager->get_proc_work_dir().find(path)) {
			// trying to remove working dir
			set_error(regs, kiv_os::erPermission_Denied);
			return;
		}

		ret_code = vfs->remove_emtpy_dir(path); // remove directory
	}

	if (ret_code != kiv_os::erSuccess) {
		set_error(regs, ret_code);
		return;
	}
}

void set_file_position(kiv_os::TRegisters &regs)
{
	std::shared_ptr<Handler> handler = processManager->get_handle_object(static_cast<kiv_os::THandle>(regs.rdx.x));
	if (!handler) {
		set_error(regs, kiv_os::erInvalid_Handle);
		return;
	}

	long offset = static_cast<long>(regs.rdi.r);
	uint8_t origin = static_cast<uint8_t>(regs.rcx.l);
	uint8_t set_size = static_cast<uint8_t>(regs.rcx.h);

	uint16_t ret_code = handler->fseek(offset, origin, set_size);
	if (ret_code != kiv_os::erSuccess) {
		set_error(regs, ret_code);
		return; // not success
	}
}

void get_file_position(kiv_os::TRegisters &regs)
{
	std::shared_ptr<Handler> handler = processManager->get_handle_object(static_cast<kiv_os::THandle>(regs.rdx.x));
	if (!handler) {
		set_error(regs, kiv_os::erInvalid_Handle);
		return;
	}

	regs.rax.r = static_cast<size_t>(handler->ftell());
}

void close_handle(kiv_os::TRegisters &regs)
{ 
	processManager->close_handle(static_cast<kiv_os::THandle>(regs.rdx.x));
}

void get_current_directory(kiv_os::TRegisters &regs) {
	char * buffer = reinterpret_cast<char*>(regs.rdx.r);
	size_t buff_size = static_cast<size_t>(regs.rcx.r);

	if (buffer == NULL || buff_size == 0) {
		set_error(regs, kiv_os::erInvalid_Argument);
		return;
	}

	std::string path = processManager->get_proc_work_dir();

	size_t len = path.size();
	if (len + 1u > buff_size) {
		set_error(regs, kiv_os::erInvalid_Argument);
		return;
	}

	strcpy_s(buffer, len + 1u, path.c_str());

	regs.rax.r = static_cast<decltype(regs.rax.r)>(len);
}

void set_current_directory(kiv_os::TRegisters &regs) {
	std::string new_path(reinterpret_cast<char*>(regs.rdx.r));

	if (new_path == "") {
		set_error(regs, kiv_os::erInvalid_Argument);
		return;
	}

	if (new_path.find(":") == std::string::npos) {
		new_path = processManager->get_proc_work_dir() + DELIMETER + new_path;
	}
	path_compiler(new_path);

	FileHandler * handler = NULL;
	uint16_t result = vfs->open_object(&handler, new_path, FS::FS_OBJECT_DIRECTORY);

	if (result == kiv_os::erSuccess && handler != NULL) {
		processManager->set_proc_work_dir(new_path);
		delete handler;
		handler = NULL;
	}
	else {
		set_error(regs, result);
	}
}

void create_pipe(kiv_os::TRegisters &regs)
{
	kiv_os::THandle* pipe_handles = reinterpret_cast<kiv_os::THandle*>(regs.rdx.r);

	Pipe *pipe = new Pipe();

	std::shared_ptr<PipeHandler> handle_write = std::make_shared<PipeHandler>(pipe, PipeHandler::fmOpen_Write);
	std::shared_ptr<PipeHandler> handle_read = std::make_shared<PipeHandler>(pipe, PipeHandler::fmOpen_Read);
	
	*pipe_handles = processManager->add_handle(handle_write);
	*(pipe_handles + 1) = processManager->add_handle(handle_read);
}


void set_error(kiv_os::TRegisters &regs, uint16_t code) {
	regs.rax.r = static_cast<decltype(regs.rax.r)>(code);
	regs.flags.carry = static_cast<decltype(regs.flags.carry)>(1);
}


void path_compiler(std::string &path)
{
	size_t bck_pos = 0;
	while ((bck_pos = path.find("..")) != std::string::npos)
	{
		size_t start_pos = path.rfind(DELIMETER, bck_pos - 2);

		if (start_pos == std::string::npos) {
			path.erase(bck_pos - DELIMETER_SIZE, 2 + DELIMETER_SIZE);
		}
		else {
			path.erase(start_pos, bck_pos - start_pos + 2);
		}
	}
	path.erase(path.find_last_not_of(PATH_ERASE_CHR) + 1);
	path.erase(0, path.find_first_not_of(PATH_ERASE_CHR));
}
