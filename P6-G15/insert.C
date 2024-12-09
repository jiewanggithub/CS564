#include "catalog.h"
#include "query.h"


/*
 * Inserts a record into the specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Insert(const string &relation, 
                       const int attrCnt, 
                       const attrInfo attrList[]) 
{
    Status status;
    int relAttrCnt;
    AttrDesc* relAttrDesc;

    // Record to be inserted
    Record record;
    RID rid;
    int recordLength = 0;

	cout << "Doing QU_Insert " << endl;

    // Retrieve metadata for the target relation
    status = attrCat->getRelInfo(relation, relAttrCnt, relAttrDesc);
    if (status != OK) {
        cerr << "Error retrieving relation information." << endl;
        return RELNOTFOUND;
    }

    // Ensure the number of attributes matches the relation's schema
    if (attrCnt != relAttrCnt) {
        return BADCATPARM;
    }

    // Calculate the total record length
    for (int i = 0; i < relAttrCnt; ++i) {
        recordLength += relAttrDesc[i].attrLen;
    }

    // Initialize the InsertFileScan for the target relation
    InsertFileScan* insertScanner = new InsertFileScan(relation, status);
    if (status != OK) {
        cerr << "Error initializing InsertFileScan." << endl;
        return UNIXERR;
    }

    record.length = recordLength;
    record.data = (char*)malloc(recordLength);

    // Match each relation attribute with the attributes being inserted
    for (int i = 0; i < relAttrCnt; ++i) {
        const AttrDesc& relAttr = relAttrDesc[i];
        bool found = false;

        for (int j = 0; j < attrCnt; ++j) {
            const attrInfo& insAttr = attrList[j];

            // Verify attribute names and types match
            bool nameMatch = (strcmp(relAttr.attrName, insAttr.attrName) == 0);
            bool typeMatch = (relAttr.attrType == insAttr.attrType);

            if (nameMatch && typeMatch) {
                if (insAttr.attrValue == NULL) {
                    cerr << "Null value encountered for an attribute." << endl;
                    return UNIXERR;
                }

                // Copy the value to the corresponding position in the record
                switch (insAttr.attrType) {
                    case STRING: {
                        char* value = (char*)insAttr.attrValue;
                        memcpy((char*)record.data + relAttr.attrOffset, value, relAttr.attrLen);
                        break;
                    }
                    case INTEGER: {
                        int value = atoi((char*)insAttr.attrValue);
                        memcpy((char*)record.data + relAttr.attrOffset, &value, relAttr.attrLen);
                        break;
                    }
                    case FLOAT: {
                        float value = atof((char*)insAttr.attrValue);
                        memcpy((char*)record.data + relAttr.attrOffset, &value, relAttr.attrLen);
                        break;
                    }
                    default:
                        cerr << "Unknown attribute type." << endl;
                        return ATTRTYPEMISMATCH;
                }

                found = true;
                break;
            }
        }

        // If a matching attribute was not found, return an error
        if (!found) {
            cerr << "Mismatch between record attributes and relation schema." << endl;
            return ATTRTYPEMISMATCH;
        }
    }

    // Insert the record into the relation
    status = insertScanner->insertRecord(record, rid);
    if (status != OK) {
        cerr << "Error inserting the record." << endl;
        return status;
    }

    // Clean up and return success
    free(record.data);
    delete insertScanner;
    return OK;
}

