/*
Name: Haolai Che
Case-ID: hxc859
file-Name: hashMap.h
Date-Created: Oct/25/2022
*/
/*
Description:
hashMap.h includes HashNode struct, HashMap struct
Defines CreateHashMap(), InsertHashMap(),GetHashMap()
DeleteHashMap(), PrintHashMap() functions, functions 
are implemented in proj4.c
*/
#ifndef _HASHMAP_H
#define _HASHMAP_H

typedef struct HashNode
{
	char* key;
	long int value;
	//int* p;
	struct HashNode* next;
}HashNode;

typedef struct
{
	int size;
	HashNode** hashArr;
}HashMap;

HashMap* CreateHashMap(int n);
int InsertHashMap(HashMap* hashMap, char* key, long int value);
int GetHashMap(HashMap* hashMap, char* key);
void DeleteHashMap(HashMap* hashMap);
void PrintHashMap(HashMap* hashMap);



#endif
