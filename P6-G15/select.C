#include "catalog.h"
#include "query.h"


// forward declaration
const Status ScanSelect(const string & result, 
			const int projCnt, 
			const AttrDesc projNames[],
			const AttrDesc *attrDesc, 
			const Operator op, 
			const char *filter,
			const int reclen);

/*
 * Selects records from the specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Select(const string & result, 
		       const int projCnt, 
		       const attrInfo projNames[],
		       const attrInfo *attr, 
		       const Operator op, 
		       const char *attrValue)
{
   // QU_Select initializes and delegates the operation to ScanSelect
    cout << "Doing QU_Select " << endl;

    Status status;
    AttrDesc attrDesc[projCnt];

    // Fetch attribute information for each projection
    for (int i = 0; i < projCnt; ++i) {
        status = attrCat->getInfo(projNames[i].relName,
                                  projNames[i].attrName,
                                  attrDesc[i]);
        if (status != OK) {
            return status;
        }
    }

    // Calculate total record length
    int recordLength = 0;
    for (int i = 0; i < projCnt; ++i) {
        recordLength += attrDesc[i].attrLen;
    }

    // Handle selection when no filtering attribute is provided
    if (attr == NULL) {
        return ScanSelect(result,
                          projCnt,
                          attrDesc,
                          NULL,
                          op,
                          NULL,
                          recordLength);
    } else {
        // Retrieve information for the filtering attribute
        AttrDesc filterAttrDesc;
        status = attrCat->getInfo(attr->relName, attr->attrName, filterAttrDesc);
        if (status != OK) return status;

        // Convert the filter value to the appropriate type and perform the selection
        switch (filterAttrDesc.attrType) {
            case INTEGER: {
                int value = atoi(attrValue);
                return ScanSelect(result,
                                  projCnt,
                                  attrDesc,
                                  &filterAttrDesc,
                                  op,
                                  reinterpret_cast<char*>(&value),
                                  recordLength);
            }

            case STRING:
                return ScanSelect(result,
                                  projCnt,
                                  attrDesc,
                                  &filterAttrDesc,
                                  op,
                                  attrValue,
                                  recordLength);

            case FLOAT: {
                float value = atof(attrValue);
                return ScanSelect(result,
                                  projCnt,
                                  attrDesc,
                                  &filterAttrDesc,
                                  op,
                                  reinterpret_cast<char*>(&value),
                                  recordLength);
            }

            default:
                // Invalid attribute type, return an error or do nothing
                break;
        }
    }
}


const Status ScanSelect(const string &result, 
                        const int projCnt, 
                        const AttrDesc projNames[],
                        const AttrDesc *attrDesc, 
                        const Operator op, 
                        const char *filter,
                        const int reclen)
{
    cout << "Doing HeapFileScan Selection using ScanSelect()" << endl;

    Status status;

    // Open the result relation and prepare for record insertion
    InsertFileScan resultRelation(result, status);
    int resultRecordCount = 0;
    if (status != OK) return status;

    // Set up the output record
    char outputBuffer[reclen];
    Record outputRecord;
    RID outputRID;
    outputRecord.data = (void*)outputBuffer;
    outputRecord.length = reclen;

    // Initialize the scanner for the source relation
    HeapFileScan scanner(string(projNames[0].relName), status);
    if (status != OK) return status;

    // Configure the scan based on whether a filtering attribute is provided
    if (attrDesc == NULL) {
        // Perform a full table scan with no filtering
        status = scanner.startScan(0, 0, STRING, NULL, EQ); // Filter defaults to all records
        if (status != OK) return status;
    } else {
        // Start scan with a filter on the specified attribute
        status = scanner.startScan(attrDesc->attrOffset,
                                   attrDesc->attrLen,
                                   (Datatype)attrDesc->attrType,
                                   filter,
                                   op);
        if (status != OK) return status;
    }

    // Iterate over matching records
    RID recordID;
    while (scanner.scanNext(recordID) == OK) {
        Record tempRecord;
        status = scanner.getRecord(tempRecord);
        assert(status == OK);

        // Extract and copy projection attributes into the output record
        int outputOffset = 0;
        for (int i = 0; i < projCnt; ++i) {
            memcpy((char*)outputRecord.data + outputOffset,
                   (char*)tempRecord.data + projNames[i].attrOffset,
                   projNames[i].attrLen);
            outputOffset += projNames[i].attrLen;
        }

        // Insert the constructed record into the result relation
        status = resultRelation.insertRecord(outputRecord, outputRID);
        assert(status == OK);
        resultRecordCount++;
    }

    return OK;
}

