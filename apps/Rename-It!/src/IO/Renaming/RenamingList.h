#pragma once
#include "Path.h"
#include "FileList.h"
#include "../KtmTransaction.h"
#include "Math/OrientedGraph.h"
#include "IOOperation/IOOperation.h"

namespace Beroux{ namespace IO{ namespace Renaming
{
	/**
	 * Handle the renaming of a set of renaming operations.
	 *
	 * Let note A->B the renaming operation of a file named "A" to "B"; here are some features supported:
	 * - The renaming order is handled internally: A->B, B->C will rename B first and then A.
	 * - Temporary file names for cycles: A->B, B->A will rename by using a temporary file name A->T, B->A, T->B.
	 * 
	 * Can detect renaming problem before renaming files, including:
	 * - System forbidden file names and most dangerous file names[1]
	 * - When two files are going to have to same file name
	 * - When files have been removed
	 *
	 * [1]: Note that "C:\program.txt" when "C:\Program Files\" exits is not checked by should give a warning
	 *      See http://www.cs.rpi.edu/courses/fall01/os/CreateProcess.html
	 *
	 * @todo Check for more dangerous file names and check if files are opened by another process.
	 */
	class CRenamingList
	{
	// Definitions
	public:
		enum EErrorLevel
		{
			levelNone,		// No error detected.
			levelWarning,	// This is a warning, so the renaming is still possible.
			levelError,		// This is an error, so the renaming would be impossible.
		};

		/**
		 * The higher, the higher the error level.
		 */
		enum EErrorCode
		{
			errNoError,				// No error.
			errDirCaseInconsistent,	// The directory's name case is different from this one for other renaming operations.
			errRiskyFileName,		// The new file name should be avoided.
			errRiskyDirectoryName,	// The new directory name should be avoided.
			errLonguerThanMaxPath,	// The full path length is >= than MAX_PATH=260 chars long (it may not work properly with some applications).
			errConflict,			// The renaming preview shows a conflict with other files (existing or in the renaming list).
			errInvalidFileName,		// The new file name is invalid.
			errInvalidDirectoryName,// The new directory name is invalid.
			errBackslashMissing,	// The path must start and end by a backslash (\).
			errFileMissing,			// The original file is missing from the storage.
			errCount
		};

		enum EStage
		{
			stageChecking,		// It's looking for possibly problems.
			stagePreRenaming,	// It's preparing the renaming (but no renaming done at this stage).
			stageRenaming,		// It's renaming the files.
		};
		enum {
			stageCount = 3		// Number of possible stages.
		};

		/**
		 * A callback function called when a renaming IOOperation was performed during renaming.
		 * The operation may be successful or a failure.
		 * \param ioOperation The operation performed.
		 * \param nErrorLevel A code indication success or failure level.
		 */
		typedef function<void (const IOOperation::CIOOperation& ioOperation, IOOperation::CIOOperation::EErrorLevel nErrorLevel)> CIOOperationPerformedEventHandler;

		/**
		 * A callback function called during the renaming to indicate progress.
		 * @param[in] nStage		The current long operation being performed.
		 * @param[in] nDone			The number of files renamed (with or without problem).
		 * @param[in] nTotal		The total number of files to be rename.
		 */
		typedef function<void (EStage nStage, int nDone, int nTotal)> CRenameProgressChangedEventHandler;

		struct CRenamingOperation
		{
			friend CRenamingList;

			CRenamingOperation()
			{
			}

			CRenamingOperation(const CPath& before, const CPath& after) :
				pathBefore(CPath::MakeUnicodePath(before.GetPath())),
				pathAfter(CPath::MakeUnicodePath(after.GetPath()))
			{
			}

			const CPath& GetPathBefore() const {
				return pathBefore;
			}

			const CPath& GetPathAfter() const {
				return pathAfter;
			}

			void SetPathBefore(const CPath& path) {
				pathBefore = CPath::MakeUnicodePath(path.GetPath());
			}

			void SetPathAfter(const CPath& path) {
				pathAfter = CPath::MakeUnicodePath(path.GetPath());
			}

		protected:
			CPath	pathBefore;
			CPath	pathAfter;
		};

		struct COperationProblem
		{
			COperationProblem()
				: nErrorLevel(levelNone)
				, nErrorCode(errNoError)
			{
			}

			EErrorLevel nErrorLevel;	// How serious is the problem (if there is any).
			EErrorCode nErrorCode;		// A code representing the general error reason.
			CString strMessage;			// A short message describing the problem's reasons (in the user language).
		};

	// Construction
		CRenamingList();
		CRenamingList(const CFileList& flBefore, const CFileList& flAfter);
		~CRenamingList();

		void Create(const CFileList& flBefore, const CFileList& flAfter);

	// Attributes
		bool IsEmpty() const { return m_vRenamingOperations.empty(); }

		/**
		 * @return Number of renaming operations to perform.
		 */
		int GetCount() const { return (int) m_vRenamingOperations.size(); }

		int GetWarningCount() const { return m_nWarnings; }

		int GetErrorCount() const { return m_nErrors; }

		/**
		 * See the declaration of CIOOperationPerformedEventHandler for more details.
		 * You can create a method void MyCallback(const IOOperation::CIOOperation& ioOperation, IOOperation::CIOOperation::EErrorLevel nErrorLevel);
		 * and then use boost::bind(&MyClass::MyCallBack, &myClassInstance, _1, _2);
		 */
		void SetIOOperationPerformedCallback(const CIOOperationPerformedEventHandler& fOnIOOperationPerformed) {
			m_fOnIOOperationPerformed = fOnIOOperationPerformed;
		}

		/**
		 * Used to track progress of renaming and other long operations.
		 * See the declaration of m_fOnProgress for more details about
		 * the callback function arguments.
		 */
		void SetProgressCallback(const CRenameProgressChangedEventHandler& fOnRenameProgress) {
			m_fOnProgress = fOnRenameProgress;
		}

		/**
		 * Return the problems found for a specific operation.
		 * Call Check() to update the problems' list.
		 */
		inline const COperationProblem& GetOperationProblem(int nIndex) const
		{
			return m_vProblems[nIndex];
		}

	// Methods to access and modify the list
		const CRenamingOperation& GetRenamingOperation(int nIndex) const {
			return m_vRenamingOperations.at(nIndex);
		}

		void AddRenamingOperation(const CRenamingOperation& roRenaming) {
			m_vRenamingOperations.push_back(roRenaming);
			m_vProblems.push_back( COperationProblem() );
		}

		void AddRenamingOperation(const CPath& pathBefore, const CPath& pathAfter) {
			AddRenamingOperation( CRenamingOperation(pathBefore, pathAfter) );
		}

		void SetRenamingOperation(int nIndex, const CRenamingOperation& roRenaming) {
			m_vRenamingOperations.at(nIndex) = roRenaming;
		}

		void RemoveRenamingOperation(int nIndex) {
			if (nIndex < 0 || nIndex >= (int) m_vRenamingOperations.size())
				throw out_of_range("Index is out of range.");

			switch (m_vProblems[nIndex].nErrorLevel)
			{
			case levelWarning:	--m_nWarnings; break;
			case levelError:	--m_nErrors; break;
			}

			m_vRenamingOperations.erase(m_vRenamingOperations.begin() + nIndex);
			m_vProblems.erase(m_vProblems.begin() + nIndex);
			ASSERT(m_vRenamingOperations.size() == m_vProblems.size());
		}

		const CRenamingOperation& operator[](int nIndex) const {
			return m_vRenamingOperations.at(nIndex);
		}

	// Operations
		/**
		 * Search for possible renaming problems before doing the renaming.
		 * After calling Check, you can GetOperationProblems().
		 * @return True when no problems (neither errors or warnings) have been detected.
		 */
		bool Check();

		/**
		 * Perform the renaming of all the file.
		 * When KTM is used and the operation fails, no file is renamed.
		 * @param ktm The class used to perform the renaming.
		 * @return True on success, false if one or more files couldn't be renamed.
		 * @note Does not commit or abort at the end.
		 */
		bool PerformRenaming(CKtmTransaction& ktm);

	// Overrides
	protected:
		virtual void OnIOOperationPerformed(const IOOperation::CIOOperation& ioOperation, IOOperation::CIOOperation::EErrorLevel nErrorLevel);

		virtual void OnProgress(EStage nStage, int nDone, int nTotal) const;

	// Implementation
	private:
		typedef vector<CRenamingOperation> CRenamingOperationList;

		// Map a drive to a temporary path and the length of the shortest common path on that drive.
		typedef map<CString, pair<CString, unsigned> > CTempDirectoryMap;

		// A comparison used to order such a set such as the longest path come first.
		template <class _Tp>
		struct path_compare : public binary_function<_Tp, _Tp, bool>
		{
			bool operator()(const _Tp& __x, const _Tp& __y) const
			{
				if (__x.GetLength() != __y.GetLength())
					return __x.GetLength() > __y.GetLength();
				else
					return __x < __y;
			}
		};

		// A comparison used to order such a set such as the shortest path come first.
		template <class _Tp>
		struct path_reverse_compare : public binary_function<_Tp, _Tp, bool>
		{
			bool operator()(const _Tp& __x, const _Tp& __y) const
			{
				if (__x.GetLength() != __y.GetLength())
					return __x.GetLength() < __y.GetLength();
				else
					return __x < __y;
			}
		};

		struct dir_case_compare : public binary_function<CString, CString, bool>
		{
			bool operator()(const CString& __x, const CString& __y) const
			{
				return __x.CompareNoCase(__y) < 0;
			}
		};

		struct DIR_CASE
		{
			DIR_CASE(int nDirLength, int nOperationIndex)
			{
				nMinDirLength = nDirLength;
				vnOperationsIndex.push_back(nOperationIndex);
			}

			int nMinDirLength;
			vector<int> vnOperationsIndex;
		};

		void CheckFileConflict(int nOperationIndex, const set<CString>& setBeforeLower, map<CString, int>& mapAfterLower);

		void CheckDirectoryPath(int nOperationIndex);

		static COperationProblem CheckName(const CString& strName, const CString& strNameWithoutExtension, bool bIsFileName);

		void SetProblem(int nOperationIndex, EErrorCode nErrorCode, CString strMessage);

		/**
		 * Prepare a PerformRenaming().
		 * \note This method may add renaming operations to m_vRenamingOperations.
		 * \param[out] setDeleteIfEmptyDirectories	An ordered set of folders that should be deleted once all renaming is complete.
		 * \return An list of operations indexes in the updated m_vRenamingOperations ordered so that
		 *         by performing them in order it should to the renaming job.
		 */
		vector<shared_ptr<IOOperation::CIOOperation> > PrepareRenaming() const;

		vector<shared_ptr<IOOperation::CIOOperation> > PrepareFileRenaming() const;

		vector<shared_ptr<IOOperation::CIOOperation> > PrepareDirectoryRenaming() const;

		static CTempDirectoryMap FindTempDirectories(const CRenamingOperationList& vRenamingOperations);

		/**
		 * Find the index of the shortest pathAfter in all the m_vRenamingOperations.
		 */
		static int FindShortestDirectoryPathAfter(CRenamingOperationList& vRenamingOperations);

		/**
		 * Detects if a directory contains some elements or not.
		 * \return true when the directory exist but it's empty, and false else.
		 */
		static bool DirectoryIsEmpty(const CString& strDirectoryPath, CKtmTransaction* pKTM = NULL);

		// Default progress callback that does nothing.
		void DefaultProgressCallback(EStage nStage, int nDone, int nTotal) {}

		CRenamingOperationList m_vRenamingOperations;
		vector<COperationProblem> m_vProblems;
		int m_nWarnings;
		int m_nErrors;
		CIOOperationPerformedEventHandler m_fOnIOOperationPerformed;
		CRenameProgressChangedEventHandler m_fOnProgress;
	};
}}}
