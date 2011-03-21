#ifndef TEST_RESOURCE_CPP
#define TEST_RESOURCE_CPP

#include "../MResource.h"
void test_resource()
{
	MResource::addFileToCache(L"test.png");
	MResource::addFileToCache(L"test.txt");
	MResource::addZipToCache(L"test.zip");
	MResource::dumpCache();
	MResource::clearCache();
}

#endif // TEST_RESOURCE_CPP
