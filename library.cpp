#include "library.h"
#include <algorithm>
#include "dirent.h"
#include <tchar.h>
#include <sstream>
#include <iterator>
#include <set>
#include <string>
#include <cctype>
#include "Shlwapi.h"
#include <regex>

//---------------------------------------------------------------------------------------------------
// tokenSplit
//---------------------------------------------------------------------------------------------------
std::vector<std::wstring> tokenSplit(const std::wstring& str, const std::wstring& delim)
{
	std::vector<std::wstring> tokens;

	std::wstring::size_type p0 = str.find_first_not_of(delim, 0);
	std::wstring::size_type p1 = str.find_first_of(delim, p0);

	while (p0 != p1 && p0 != std::wstring::npos)
	{
		tokens.push_back(str.substr(p0, p1 - p0));

		p0 = str.find_first_not_of(delim, p1);
		p1 = str.find_first_of(delim, p0);
	}

	return tokens;
}

//---------------------------------------------------------------------------------------------------
// tokenMerge
//---------------------------------------------------------------------------------------------------
std::wstring tokenMerge(const std::vector<std::wstring>& tokens, const std::wstring& delim)
{
	std::wstring tokenString;

	for (std::vector<std::wstring>::const_iterator itr = tokens.begin(); itr != tokens.end(); ++itr)
	{
		if (itr == tokens.begin())
		{
			tokenString += *itr;
		}
		else
		{
			tokenString += delim + *itr;
		}
	}

	return tokenString;
}

//---------------------------------------------------------------------------------------------------
// toLower
//---------------------------------------------------------------------------------------------------
std::wstring toLower(const std::wstring& str)
{
	std::wstring temp = str;
	std::transform(str.begin(), str.end(), temp.begin(), ::tolower);
	//std::transform(str.begin(), str.end(), temp.begin(), [](unsigned char c) -> unsigned char { return std::toupper(c); });

	return temp;
}

//---------------------------------------------------------------------------------------------------
// hashString
// FNV www.isthe.com/chongo/tech/comp/fnv/
//---------------------------------------------------------------------------------------------------
unsigned int hashString(const std::wstring& str)
{
	static const unsigned int magic = 0x01000193;

	unsigned char *s = (unsigned char *)str.c_str();

	unsigned int hval = 0;

	unsigned int count = str.size() * sizeof(TCHAR);

	for (int i=0; i!=count; ++i)
	{
		hval *= magic;
		hval ^= *s++;
	}

	return hval;
}

//---------------------------------------------------------------------------------------------------
// hackyIsFolder
//---------------------------------------------------------------------------------------------------
bool hackyIsFolder(const std::wstring& path)
{
	int lastSlashPos = path.rfind('\\');
	int lastDotPos = path.rfind('.');

	if (lastSlashPos == path.length())
		return true;

	if (lastDotPos == -1)
		return true;

	if (lastDotPos > lastSlashPos)
		return false;

	return false;
}


//---------------------------------------------------------------------------------------------------
// Library
//---------------------------------------------------------------------------------------------------
Library::Library(const std::wstring& savePath)
{
	m_SavePath = savePath;
	m_SavePath += L"\\";
	m_SavePath += L"VGPLibrarian.cfg";
}

//---------------------------------------------------------------------------------------------------
// AddScanPath
//---------------------------------------------------------------------------------------------------
void Library::AddScanPath(const std::wstring& rootPath, bool runScan)
{
	std::wstring path = toLower(rootPath);

	if (path.rfind('\\') != path.length())
	{
		path += L"\\";
	}

	if (std::find(m_ScanPaths.begin(), m_ScanPaths.end(), path) == m_ScanPaths.end())
	{
		m_ScanPaths.push_back(path);
	}

	if (runScan)
	{
		ScanFolder(path);
	}
}

//---------------------------------------------------------------------------------------------------
// Scan
//---------------------------------------------------------------------------------------------------
void Library::Scan()
{
	for (PathArray::iterator itr = m_ScanPaths.begin(); itr != m_ScanPaths.end(); ++itr)
	{
		ScanFolder((*itr).c_str());
	}
}

//---------------------------------------------------------------------------------------------------
// Clear
//---------------------------------------------------------------------------------------------------
void Library::Clear()
{
	for (DocumentList::const_iterator itr = m_Documents.GetDocuments().begin(); itr != m_Documents.GetDocuments().end(); ++itr)
	{
		delete (*itr);
	}

	m_Documents.Clear();
}

//---------------------------------------------------------------------------------------------------
// ScanFolder
//---------------------------------------------------------------------------------------------------
void Library::ScanFolder(const std::wstring& path)
{
    _WDIR* dir = _wopendir(path.c_str());

	if (!dir)
		return;

	struct _wdirent* dirEntry;

	while ((dirEntry = _wreaddir(dir)) != NULL)
	{
		std::wstring fullPath = path + _T("\\") + dirEntry->d_name;

		switch (dirEntry->d_type)
		{
			case DT_REG:
			{
				AddDocument(fullPath, L"", false);
				break;
			}

			case DT_DIR:
				if (wcscmp(dirEntry->d_name, L".") != 0 && wcscmp(dirEntry->d_name, L"..") != 0) 
				{
					ScanFolder(fullPath);
				}
			break;
		}
	}

	wclosedir(dir);
}

//---------------------------------------------------------------------------------------------------
// LoadMeta
//---------------------------------------------------------------------------------------------------
void Library::LoadMeta()
{
	FILE* hMetaFile;
	_wfopen_s(&hMetaFile, m_SavePath.c_str(), L"r,ccs=UTF-8");

	if (!hMetaFile)
		return;

	WCHAR buffer[1024];
	while (fgetws(buffer, 1024, hMetaFile))
	{
		std::wstring line = buffer;

		std::wstring::size_type quote1 = line.find(L"\"", 0);
		std::wstring::size_type quote2 = line.find(L"\"", quote1+1);
		std::wstring::size_type comma1 = line.find(L",", quote2 + 1);
		std::wstring::size_type newline = line.find(L"\n");


		std::wstring path = line.substr(quote1 + 1, quote2 - quote1 - 1);
		std::wstring keywords = line.substr(comma1 + 1, newline - comma1 - 1);

		AddDocument(path, keywords, false);
	}

	fclose(hMetaFile);
}

//---------------------------------------------------------------------------------------------------
// SaveMeta
//---------------------------------------------------------------------------------------------------
void Library::SaveMeta() const
{
	FILE* hFile;
	_wfopen_s(&hFile, m_SavePath.c_str(), L"w,ccs=UTF-8");

	if (!hFile)
		return;

	for (PathArray::const_iterator itr = m_ScanPaths.begin(); itr != m_ScanPaths.end(); ++itr)
	{
		fwprintf(hFile, L"\"%s\"\n", (*itr).c_str());
	}

	for (DocumentList::const_iterator itr = m_Documents.GetDocuments().begin(); itr != m_Documents.GetDocuments().end(); ++itr)
	{
		fwprintf(hFile, L"\"%s\",%s\n", (*itr)->m_path.c_str(), (*itr)->m_keywords.c_str());
	}

	fclose(hFile);
}

//---------------------------------------------------------------------------------------------------
// AddDocument
//---------------------------------------------------------------------------------------------------
bool Library::AddDocument(Document& doc)
{
	return m_Documents.AddDocument(doc);
}

//---------------------------------------------------------------------------------------------------
// RemoveDocument
//---------------------------------------------------------------------------------------------------
Document* Library::AddDocument(const std::wstring& path, const std::wstring& keywords, bool runScanOnFolders)
{
	if (path.size() < 1)
		return NULL;

	Document* doc = NULL;

	if (PathIsURL(path.c_str()))
	{
		doc = new Document();

		doc->m_path = path;
		doc->m_pathLower = toLower(doc->m_path);
		doc->SetKeywords(keywords);

		doc->m_hash = hashString(path);

		doc->m_timestamp = L"";
	}
	else
	{
		//WIN32_FILE_ATTRIBUTE_DATA fileInfo;

		//if (!GetFileAttributesEx(path.c_str(), GetFileExInfoStandard, &fileInfo))
		//	return NULL;

		//if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		if (hackyIsFolder(path))
		{
			AddScanPath(path, runScanOnFolders);
			return NULL;
		}

		doc = new Document();

		doc->m_path = path;
		doc->m_pathLower = toLower(doc->m_path);
		doc->SetKeywords(keywords);

		doc->m_hash = hashString(path);
	}

	Document* existingDoc = NULL;

	if (doc && !m_Documents.AddDocument(*doc, &existingDoc))
	{
		delete doc;
		doc = existingDoc;
	}

	return doc;
}

//---------------------------------------------------------------------------------------------------
// RemoveDocument
//---------------------------------------------------------------------------------------------------
void Library::RemoveDocument(const std::wstring& path)
{
	Document* ptr = m_Documents.RemoveDocument(path);

	if (ptr)
	{
		delete ptr;
	}
}

//---------------------------------------------------------------------------------------------------
// Clear
//---------------------------------------------------------------------------------------------------
void Documents::Clear()
{
	m_Documents.clear();
}

//---------------------------------------------------------------------------------------------------
// AddDocument
//---------------------------------------------------------------------------------------------------
bool Documents::AddDocument(Document& doc, Document** existingDoc)
{
	if (existingDoc)
	{
		(*existingDoc) = NULL;
	}

	for (DocumentList::iterator itr = m_Documents.begin(); itr != m_Documents.end(); ++itr)
	{
		if ((*itr)->m_hash == doc.m_hash || ((*itr)->m_pathLower.size() == doc.m_pathLower.size() && (*itr)->m_pathLower == doc.m_pathLower))
		{
			// Merge keywords
			std::set<std::wstring> keywords;

			std::vector<std::wstring> set1 = tokenSplit((*itr)->m_keywords);
			std::vector<std::wstring> set2 = tokenSplit(doc.m_keywords);
			std::vector<std::wstring> set3 = tokenSplit(doc.m_pathLower, L"\\/");

			keywords.insert(set1.begin(), set1.end());
			keywords.insert(set2.begin(), set2.end());
			keywords.insert(set3.begin(), set3.end());

			std::vector<std::wstring> setFinal(keywords.begin(), keywords.end());

			(*itr)->m_keywords = tokenMerge(setFinal);

			if (existingDoc)
			{
				(*existingDoc) = (*itr);
			}

			return false;
		}
	}

	m_Documents.push_back(&doc);

	return true;
}

//---------------------------------------------------------------------------------------------------
// RemoveDocument
//---------------------------------------------------------------------------------------------------
Document* Documents::RemoveDocument(const std::wstring& path)
{
	std::wstring pathLower = toLower(path);

	for (DocumentList::iterator itr = m_Documents.begin(); itr != m_Documents.end(); ++itr)
	{
		if ( (*itr)->m_pathLower == pathLower)
		{
			Document* ptr = (*itr);
			m_Documents.erase(itr);
			return ptr;
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------------------------------
// Filter
//---------------------------------------------------------------------------------------------------
Documents Documents::Filter(const std::wstring& filterString) const
{
	if (filterString.empty())
		return m_Documents;


	std::vector<std::wstring> filterTokens1 = tokenSplit(toLower(filterString));
	std::set<std::wstring> filterTokens(filterTokens1.begin(), filterTokens1.end());

	Documents results;

	for (DocumentList::const_iterator itr = m_Documents.begin(); itr != m_Documents.end(); ++itr)
	{
		bool match = true;

		for (std::set<std::wstring>::iterator itr1 = filterTokens.begin(); itr1 != filterTokens.end(); ++itr1)
		{
			std::wstring subStr = (*itr1);

			if (	(*itr)->m_pathLower.find(subStr) == std::wstring::npos &&
					(*itr)->m_keywords.find(subStr) == std::wstring::npos)
			{
				match = false;
				break;
			}
		}

		if (match)
		{
			results.m_Documents.push_back(*itr);
		}
	}

	return results;
}

//---------------------------------------------------------------------------------------------------
// SetKeywords
//---------------------------------------------------------------------------------------------------
void Document::SetKeywords(const std::wstring& keywords)
{
	std::wstring keywordString = toLower(keywords);

	m_keywords.clear();
	m_authors.clear();
	m_company.clear();

	// Scan and extract author tags
	if (keywords.find(L"author") != -1)
	{
		std::wregex authorRegex(L"author\\s*:\\s*\\{(.*?)\\}");
	
		std::wstring keywordsWithoutAuthors;

		std::vector<std::wstring> authors;

		std::wsmatch results;
		while (std::regex_search(keywordString, results, authorRegex))
		{
 			authors.push_back(results[1].str());

			keywordsWithoutAuthors += results.prefix();
			keywordString = results.suffix();
		}

		keywordsWithoutAuthors += keywordString;

		keywordString = keywordsWithoutAuthors;

		std::sort(authors.begin(), authors.end());

		for (auto author: authors)
		{
			m_keywords += L"author:{" + author + L"}, ";
		}

		m_authors = tokenMerge(authors);
	}

	// Scan and extract company tags
	if (keywords.find(L"company") != -1)
	{
		std::wregex companyRegex(L"company\\s*:\\s*\\{(.*?)\\}");
	
		std::wstring keywordsWithoutCompanies;

		std::vector<std::wstring> companies;

		std::wsmatch results;
		while (std::regex_search(keywordString, results, companyRegex))
		{
 			companies.push_back(results[1].str());

			keywordsWithoutCompanies += results.prefix();
			keywordString = results.suffix();
		}

		keywordsWithoutCompanies += keywordString;

		keywordString = keywordsWithoutCompanies;

		std::sort(companies.begin(), companies.end());

		for (auto company: companies)
		{
			m_keywords += L"company:{" + company + L"}, ";
		}

		m_company = tokenMerge(companies);
	}

	// Split the rest into unique keywords
	{
		std::vector<std::wstring> v = tokenSplit(keywordString);

		std::set<std::wstring> s;
		s.insert(v.begin(), v.end());

		std::vector<std::wstring> unique(s.begin(), s.end());
		m_keywords += tokenMerge(unique);
	}
}
