#pragma once

#include "RenamingList.h"

/**
 * Report errors and ask the user to fix them.
 */
class CReport
{
// Construction
public:
	CReport(void);
	~CReport(void);

// Operations
	/**
	 * Display the renaming errors with their meaning and ask the user to fix them.
	 * @param[in,out] renamingList	A renaming list.
	 * @param[in,out] uvErrorFlag	A list of errors reported by CRenamingManager::FindErrors().
	 * @return True if there are no more errors (meaning there was none or they have been fixed).
	 */
	bool ShowReportFixErrors(CRenamingList& renamingList, vector<unsigned>& uvErrorFlag);
};
