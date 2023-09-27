#pragma once
#include "result_common.hpp"

DEFINE_HORIZON_RESULT_MODULE(Result::FS, FS);

namespace Result::FS {
	// TODO: Verify this
	DEFINE_HORIZON_RESULT(FileNotFound, 100, NotFound, Status);
	// TODO: Verify this
	DEFINE_HORIZON_RESULT(FileNotFoundAlt, 112, NotFound, Status);
	// Also a not found error code used here and there in the FS module.
	DEFINE_HORIZON_RESULT(NotFoundInvalid, 120, InvalidState, Status);
	DEFINE_HORIZON_RESULT(AlreadyExists, 190, NothingHappened, Info);
	DEFINE_HORIZON_RESULT(FileTooLarge, 210, OutOfResource, Info);
	// Trying to access an archive that needs formatting and has not been formatted
	DEFINE_HORIZON_RESULT(NotFormatted, 340, InvalidState, Status);
	DEFINE_HORIZON_RESULT(UnexpectedFileOrDir, 770, NotSupported, Usage);

	// Trying to rename a file that doesn't exist or is a directory
	DEFINE_HORIZON_RESULT(RenameNonexistentFileOrDir, 120, NotFound, Status);

	// Trying to rename a file but the destination already exists
	DEFINE_HORIZON_RESULT(RenameFileDestExists, 190, NothingHappened, Status);
};  // namespace Result::FS
