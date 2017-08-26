#pragma once

#include <string>
#include <list>
#include <vector>
#include <map>

//---------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------
std::vector<std::wstring> tokenSplit(const std::wstring& str, const std::wstring& delim = L", \t");
std::wstring tokenMerge(const std::vector<std::wstring>& tokens, const std::wstring& delim = L", ");
std::wstring toLower(const std::wstring& str);

//---------------------------------------------------------------------------------------------------
// struct Document
//---------------------------------------------------------------------------------------------------
struct Document
{
	std::wstring	m_path;
	std::wstring	m_pathLower;
	std::wstring	m_keywords;
	unsigned int	m_hash;
	std::wstring	m_timestamp;

	void SetKeywords(const std::wstring& keywordString);
};

typedef std::list<Document*> DocumentList;

//---------------------------------------------------------------------------------------------------
// class Documents
//---------------------------------------------------------------------------------------------------
class Documents
{
public:

	Documents()
	{}

	Documents(const DocumentList& list)
		: m_Documents(list)
	{}

	void Clear();
	
	bool Empty() const
	{
		return m_Documents.size() == 0;
	}

	Documents Filter(const std::wstring& filterString) const;

	bool AddDocument(Document& doc, Document** existingDoc = NULL);
	Document* RemoveDocument(const std::wstring& path);

	const DocumentList&	GetDocuments() const
	{
		return m_Documents;
	}

private:
	DocumentList	m_Documents;
};

//---------------------------------------------------------------------------------------------------
// class Library
//---------------------------------------------------------------------------------------------------
class Library
{
public:

	Library(const std::wstring& savePath);

	const std::wstring& GetSaveFilename() const
	{
		return m_SavePath;
	}

	void AddScanPath(const std::wstring& rootPath, bool runScan);
	
	void Scan();

	void Clear();

	void LoadMeta();
	void SaveMeta() const;

	bool AddDocument(Document& doc);
	Document* AddDocument(const std::wstring& path, const std::wstring& keywords, bool runScanOnFolders);
	void RemoveDocument(const std::wstring& path);

	const DocumentList& GetDocuments() const
	{
		return m_Documents.GetDocuments();
	}

	Documents Filter(const std::wstring& filterString) const
	{
		return m_Documents.Filter(filterString);
	}

private:
	void ScanFolder(const std::wstring& path);

private:
	typedef std::vector<std::wstring> PathArray;

	std::wstring	m_SavePath;
	PathArray		m_ScanPaths;
	Documents		m_Documents;

};