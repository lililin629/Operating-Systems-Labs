#include <iostream>
#include <stdio.h>
#include <fstream>
#include <cstring>
#include <string.h>
#include <string>
#include <vector>
// TODO:
#include <bits/stdc++.h>

using namespace std;

ifstream file;
int lineNum = 0;
int offset;
char line[4096];
bool nextLine = true;
string prevTok;

class Symbol
{
public:
    string symName;
    int rltvAddr;
    int absAddr;
    int moduleNum;
    bool multiDefined;
    bool used;
};
vector<Symbol> symTable;
vector<Symbol> defList;

class Token
{
public:
    string tokName;
    int tokLine;
    int tokOffset;

    void setToken(string tokenStr, int lineNo, int lineOff)
    {
        tokName = tokenStr;
        tokLine = lineNo;
        tokOffset = lineOff;
    }
};

Token getToken()
{
    char *token;
    Token t;
    int lastOffset;
    int lineLen;

    while (true)
    {
        while (nextLine)
        {
            file.getline(line, sizeof(line));

            lineNum++;
            token = strtok(line, " \t\n");
            if (token)
            {
                nextLine = false;
                offset = token - line + 1;
                t.setToken(token, lineNum, offset);
                return t;
            }
            else
            {
                if (file.eof())
                {
                    if (lineLen == 0 && lineNum != 2)
                    {
                        t.setToken("eof", lineNum - 1, 1);
                    }
                    else
                    {
                        t.setToken("eof", lineNum - 1, offset + prevTok.length());
                    }
                    return t;
                }
            }
            lineLen = strlen(line);
        }
        token = strtok(NULL, " \t\n");
        if (token != NULL)
        {
            // calculate offset of next token
            offset = token - line + 1;
            // construct next toekn
            t.setToken(token, lineNum, offset);
            return t;
        };
        nextLine = true;
    };
};

void __parseerror(int errcode, Token tok)
{
    string errstr[] = {
        "NUM_EXPECTED",
        "SYM_EXPECTED",
        "ADDR_EXPECTED",
        "SYM_TOO_LONG",
        "TOO_MANY_DEF_IN_MODULE",
        "TOO_MANY_USE_IN_MODULE",
        "TOO_MANY_INSTR",
    };
    // TODO: delete tok.tokName
    cout << "Parse Error line " << tok.tokLine << " offset " << tok.tokOffset << ": " << errstr[errcode] << endl;
    exit(0);
};

Token readInt()
{
    Token tok = getToken();
    if (tok.tokName == "eof")
    {
        __parseerror(0, tok);
    }
    int i;
    try
    {
        i = stoi(tok.tokName);
    }
    catch (invalid_argument const &e)
    {
        __parseerror(0, tok);
    }
    // anything >= 2^30 is not a number
    catch (out_of_range const &e)
    {
        __parseerror(0, tok);
    }
    return tok;
};

Token readDefCount()
{
    Token tok = getToken();
    if (tok.tokName == "eof")
        return tok;
    int i;
    try
    {
        i = stoi(tok.tokName);
    }
    catch (invalid_argument const &e)
    {
        __parseerror(0, tok);
    }
    // anything >= 2^30 is not a number
    catch (out_of_range const &e)
    {
        __parseerror(0, tok);
    }
    return tok;
};

Token readSymbol()
{
    Token tok = getToken();
    if (tok.tokName == "eof")
    {
        __parseerror(1, tok);
    }
    if (tok.tokName.length() < 1)
    {
        __parseerror(1, tok);
    }
    for (int i = 0; i < tok.tokName.length(); i++)
    {
        if (i == 0)
        {
            if (!isalpha(tok.tokName[i]))
                __parseerror(1, tok);
        }
        else
        {
            if (!isalnum(tok.tokName[i]))
                __parseerror(1, tok);
        }
    };
    if (tok.tokName.length() > 16)
        __parseerror(3, tok);

    return tok;
}

Token readIAER()
{
    Token tok = getToken();
    if (tok.tokName == "eof")
    {
        __parseerror(2, tok);
    }
    if (tok.tokName.length() > 1)
    {
        __parseerror(2, tok);
    }
    if (tok.tokName[0] != 'I' && tok.tokName[0] != 'A' && tok.tokName[0] != 'E' && tok.tokName[0] != 'R')
    {

        __parseerror(2, tok);
    }

    return tok;
}

void parse1(string fileName)
{
    file.open(fileName);
    if (!file)
    {
        cout << "File not found: " << fileName << endl;
        exit(0);
    }
    int moduleNum = 0;
    int baseAddr = 0;
    int totalInst = 0;

    // while not end of file
    while (!file.eof())
    {
        moduleNum++;
        defList.clear();

        // deflist
        Token curTok = readDefCount();
        prevTok = curTok.tokName;
        if (curTok.tokName == "eof")
            break;
        int defCount = stoi(curTok.tokName);
        if (defCount > 16)
        {
            __parseerror(4, curTok);
        };
        for (int i = 0; i < defCount; i++)
        {
            curTok = readSymbol();
            prevTok = curTok.tokName;
            string symName = curTok.tokName;
            curTok = readInt();
            prevTok = curTok.tokName;
            int rltvAddr = stoi(curTok.tokName);
            int absAddr = baseAddr + rltvAddr;

            Symbol s;
            s.symName = symName;
            s.rltvAddr = rltvAddr;
            s.absAddr = absAddr;
            s.moduleNum = moduleNum;
            s.multiDefined = false;
            s.used = false;

            for (int i = 0; i < symTable.size(); i++)
            {
                if (s.symName.compare(symTable[i].symName) == 0)
                {
                    symTable[i].multiDefined = true;
                    s.multiDefined = true;
                    break;
                }
            }
            defList.push_back(s);
            if (!s.multiDefined)
            {
                symTable.push_back(s);
            }
        }

        // uselist
        curTok = readInt();
        prevTok = curTok.tokName;
        int useCount = stoi(curTok.tokName);
        if (useCount > 16)
        {
            __parseerror(5, curTok);
        };
        for (int i = 0; i < useCount; i++)
        {
            curTok = readSymbol();
            prevTok = curTok.tokName;
        }

        // instList
        curTok = readInt();
        prevTok = curTok.tokName;
        int instCount = stoi(curTok.tokName);
        totalInst += instCount;
        if (totalInst > 512)
        {
            __parseerror(6, curTok);
        };
        for (int i = 0; i < instCount; i++)
        {
            curTok = readIAER();
            prevTok = curTok.tokName;
            curTok = readInt();
            prevTok = curTok.tokName;
        }

        // print Warnings
        for (int p = 0; p < defList.size(); p++)
        {
            if (defList[p].multiDefined)
                cout << "Warning: Module " << moduleNum << ": " << defList[p].symName << " redefined and ignored" << endl;
            if (defList[p].rltvAddr > instCount)
                cout << "Warning: Module " << moduleNum << ": " << defList[p].symName
                     << " too big " << defList[p].rltvAddr << " (max=" << instCount - 1
                     << ") assume zero relative" << endl;
            if (!defList[p].multiDefined && defList[p].rltvAddr > instCount)
            {
                for (int q = 0; q < symTable.size(); q++)
                {
                    if (symTable[q].symName == defList[p].symName)
                    {
                        symTable[q].absAddr = baseAddr;
                    }
                }
            }
        }
        baseAddr += instCount;
    };

    // print Symbol Table
    cout << "Symbol Table" << endl;
    for (int p = 0; p < symTable.size(); p++)
    {
        cout << symTable[p].symName << "=" << symTable[p].absAddr;
        if (symTable[p].multiDefined)
        {
            cout << " Error: This variable is multiple times defined; first value used";
        }
        cout << endl;
    }
    cout << endl;
    file.close();
}

void parse2(string fileName)
{
    file.open(fileName);
    if (!file)
    {
        cout << "File not found: " << fileName << endl;
        exit(0);
    }
    int moduleNum = 0;
    int instCount = 0;
    int baseAddr = 0;
    // int totalInst = 0;

    cout << "Memory Map" << endl;
    while (!file.eof())
    {
        vector<string> useList;
        vector<string> usedInE;
        moduleNum++;
        // defList
        Token curTok = readDefCount();
        if (curTok.tokName == "eof")
            break;
        int defCount = stoi(curTok.tokName);
        for (int i = 0; i < defCount; i++)
        {
            curTok = readSymbol();
            curTok = readInt();
        }

        // useList: get vector<Symbol> useList, useLen = useList.size()
        curTok = readInt();
        int useCount = stoi(curTok.tokName);
        for (int i = 0; i < useCount; i++)
        {
            curTok = readSymbol();
            useList.push_back(curTok.tokName);
        }

        // instList
        // moduleSize = instCount
        // for (instCount) do:
        //     instMod = readIAER()
        //     inst = readInt()
        //     if I
        //     if A
        //     if E (update symTable => s.used = true)
        //     if R
        curTok = readInt();
        int moduleSize = stoi(curTok.tokName);
        for (int i = 0; i < moduleSize; i++)
        {
            curTok = readIAER();
            string instMod = curTok.tokName;
            curTok = readInt();
            int inst = stoi(curTok.tokName);
            int operand = inst % 1000;

            if (instMod == "I")
            {
                if (inst >= 10000)
                {
                    inst = 9999;
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst << " "
                         << "Error: Illegal immediate value; treated as 9999" << endl;
                }
                else
                {
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst << endl;
                }
            }
            if (instMod == "A")
            {
                if (inst >= 10000)
                {
                    inst = 9999;
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst << " "
                         << "Error: Illegal opcode; treated as 9999" << endl;
                }
                else if (operand > 512)
                {
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst - operand << " "
                         << "Error: Absolute address exceeds machine size; zero used" << endl;
                }
                else
                {
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst << endl;
                }
            }
            if (instMod == "E")
            {
                if (inst >= 10000)
                {
                    inst = 9999;
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst << " "
                         << "Error: Illegal opcode; treated as 9999" << endl;
                }
                else if (operand >= useList.size())
                {
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst << " "
                         << "Error: External address exceeds length of uselist; treated as immediate" << endl;
                }
                else
                {
                    usedInE.push_back(useList[operand]);
                    bool isDefined = false;
                    for (int m = 0; m < symTable.size(); m++)
                    {
                        if (symTable[m].symName.compare(useList[operand]) == 0)
                        {
                            symTable[m].used = true;
                            isDefined = true;
                            cout << setfill('0') << setw(3) << instCount << ": "
                                 << setfill('0') << setw(4) << inst - operand + symTable[m].absAddr << endl;
                        }
                    }
                    if (!isDefined)
                    {
                        cout << setfill('0') << setw(3) << instCount << ": "
                             << setfill('0') << setw(4) << inst - operand << " "
                             << " Error: " << useList[operand] << " is not defined; zero used" << endl;
                    }
                }
            }
            if (instMod == "R")
            {
                if (inst >= 10000)
                {
                    inst = 9999;
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst << " "
                         << "Error: Illegal opcode; treated as 9999" << endl;
                }
                else if (operand > moduleSize)
                {
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst - operand + baseAddr << " "
                         << "Error: Relative address exceeds module size; zero used" << endl;
                }
                else
                {
                    cout << setfill('0') << setw(3) << instCount << ": "
                         << setfill('0') << setw(4) << inst + baseAddr << endl;
                }
            }
            instCount++;
        }
        baseAddr += moduleSize;
        // Warning(rule7) for each sym in useList: if not in usedInE => warn!
        for (int m = 0; m < useList.size(); m++)
        {
            string ele = useList[m];
            if (find(usedInE.begin(), usedInE.end(), ele) == usedInE.end())
            {
                cout << "Warning: Module " << moduleNum << ": " << useList[m]
                     << " appeared in the uselist but was not actually used" << endl;
            }
        }
    }
    // Warning(rule4)
    // for each sym in deflist: if not s.used => warn
    cout << endl;
    for (int m = 0; m < symTable.size(); m++)
    {
        Symbol s = symTable[m];
        if (!s.used)
        {
            cout << "Warning: Module " << symTable[m].moduleNum << ": " << symTable[m].symName
                 << " was defined but never used" << endl;
        }
    }
    file.close();
}

int main(int argc, char *argv[])
{
    string fileName = argv[1];
    parse1(fileName);
    parse2(fileName);

    return 0;
}
