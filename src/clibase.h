// clibase.h: cli code shared by core & server.
// (c) Wikimedia Deutschland, written by Johannes Kroll in 2011, 2012
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef CLIBASE_H
#define CLIBASE_H

using namespace std;

#ifndef _
#define _(string) gettext(string)
#endif

#define _stringize(x) #x
#define stringify(x) _stringize(x)

// convenience macros for printing success/failure/error messages from the cli.
// if the protocol for those messages should ever have to be modified, change these.
#define SUCCESS_STR "OK."
#define FAIL_STR "FAILED!"
#define ERROR_STR "ERROR!"
#define NONE_STR "NONE."
#define DENIED_STR "DENIED!"    // only used in server.
#define VALUE_STR "VALUE:"
#define cliMessage(str, x...) ({ char c[2048]; int n= sprintf(c, str " "); snprintf(c+n, sizeof(c)-n, x); lastStatusMessage= c; })
#define cliSuccess(x...) cliMessage(SUCCESS_STR, x)
#define cliFailure(x...) cliMessage(FAIL_STR, x)
#define cliError(x...) cliMessage(ERROR_STR, x)
#define cliNone(x...) cliMessage(NONE_STR, x)
#define cliValue(x...) cliMessage(VALUE_STR, x)

// bump this up if any part of the protocol between graphcore and graphserv changes.
#define PROTOCOL_VERSION    4

// list of core command status codes.
// server and core include these in an enum.
#define CORECMDSTATUSCODES                                              \
    CMD_SUCCESS,        /* command succeeded */                         \
    CMD_FAILURE,        /* command failed, graph did not change */      \
    CMD_ERROR,          /* command failed, graph may have changed */    \
    CMD_NONE,           /* command succeeded, but no answer to query was found */   \
    CMD_VALUE           /* a value is returned in-line (*-meta commands) */

// list of all core command status strings, in the same order as the codes above.
// needed for translation to and from integer status codes.
#define CORECMDSTATUSSTRINGS  \
        SUCCESS_STR, FAIL_STR, ERROR_STR, NONE_STR, VALUE_STR

inline void chomp(char *line) { int n= strlen(line); if(n && line[n-1]=='\n') line[n-1]= 0; }

inline std::string format(const char *fmt, ...)
{
    char c[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c, sizeof(c), fmt, ap);
    va_end(ap);
    return string(c);
}

// abstract base class for cli commands
class CliCommand
{
    public:
        enum ReturnType
        {
            RT_NONE,
            RT_ARC_LIST,
            RT_NODE_LIST,
            RT_OTHER,
        };

        CliCommand() { }
        virtual ~CliCommand() { }

        // the command name
        virtual string getName()            { return "CliCommand"; }
        // one line describing the command and its parameters
        virtual string getSynopsis()        { return getName(); }
        // help text describing the function of the command
        virtual string getHelpText()        { return "Help text for " + getName() + "."; }
        virtual void syntaxError()
        {
            lastStatusMessage= string(FAIL_STR) + _(" Syntax: ") + getSynopsis() + "\n";
        }
        const string &getStatusMessage()    { return lastStatusMessage; }
        virtual ReturnType getReturnType()= 0;

        // read a data set of node IDs.
        // expectedSize: expected size of set per line (e. g. 1 for nodes, 2 for arcs)
        // update lastErrorString and return true on success, false on failure.
        bool readNodeset(FILE *inFile, vector< vector<uint32_t> > &dataset, unsigned expectedSize);


    protected:
        string lastStatusMessage;
};



class Cli
{
    public:
        Cli() {}

        virtual ~Cli()
        {
            for(unsigned i= 0; i<commands.size(); i++)
                delete(commands[i]);
            commands.clear();
        }

        CliCommand *findCommand(string name)
        {
            for(unsigned i= 0; i<commands.size(); i++)
                if(commands[i]->getName()==name) return commands[i];
            return 0;
        }

        vector<CliCommand*> &getCommands() { return commands; }

        // convert string to unsigned int
        static uint32_t parseUint(string str)
        {
            return strtoul(str.c_str(), 0, 0);
        }

        // check if string forms a valid unsigned integer
        static bool isValidUint(const string& s)
        {
            // disallow empty strings
            if(s.length()<1) return false;
            // allow only positive decimal digits
            for(size_t i= 0; i<s.length(); i++)
                if( !isdigit(s[i]) ) return false;
            return true;
        }

        // check if string forms a valid node (vertex) id.
        static bool isValidNodeID(const string& s)
        {
            return isValidUint(s) && parseUint(s)!=0;
        }

        // parse a data record in text form, check for valid uints
        // throws runtime_error
        static void readUintRecord(FILE *f, vector<uint32_t> &ret)
        {
            char line[1024]; 
            uint32_t n;
            if(fgets(line, 1024, f)==0)
            {
                if(!feof(f)) throw runtime_error( _("fgets failed: ") + string(strerror(errno)) );    //return false;
                else return;
            }
            filterNewlines(line);    // filter out any CRLFs
            if( (n= strlen(line)) && line[n-1]=='\n' ) line[--n]= 0;    // chomp line buffer.
            vector<string> strings= splitString(line);
            if(strlen(line) && strings.empty())
                throw runtime_error(_("a non-empty string with no words (i. e. entirely made up of delimiters) is illegal in an integer record"));
                // return false;
            for(uint32_t i= 0; i<strings.size(); i++)
            {
                if(!isValidUint(strings[i])) throw runtime_error( strings[i] + _(" is not a uint") ); //return false;
                ret.push_back(parseUint(strings[i]));
            }
            return;
        }

        // like readUintRecord(), and also check that integers are in valid range for node IDs (currently 1..uint32_max)
        // throws runtime_error
        static void readNodeIDRecord(FILE *f, vector<uint32_t> &ret)
        {
            readUintRecord(f, ret);
            for(vector<uint32_t>::iterator i= ret.begin(); i!=ret.end(); ++i)
                if(*i==0) throw runtime_error(_("node IDs must be non-zero")); //return false;
        }

        // split a string into words using given delimiters
        static vector<string> splitString(const char *s, const char *delim= " \n\t,")
        {
            vector<string> ret;
            const char *str= s;
            while(true)
            {
                while(*str && strchr(delim, *str)) str++;
                if(!*str) return ret;
                const char *start= str;
                while(*str && !strchr(delim, *str)) str++;
                ret.push_back(string(start, str-start));
                if(*str) str++;
            }
        }



    protected:
        vector<CliCommand*> commands;

        // convert crlf newlines to lf.
        static char *filterNewlines(char *line)
        {
            if(!line) return 0;
            int i= 0, di= 0;
            do {
                line[di]= line[i];
                if(line[i]!='\r') di++;
            } while(line[i++]);
            return line;
        }

        // read a line from stdin.
        // return 0 on error
        virtual char *getLine()
        {
            char *linebuf= (char*)malloc(1024);
            if(!linebuf) return 0;
            char *l= fgets(linebuf, 1024, stdin);
            if(!l) { free(linebuf); return 0; }
            return filterNewlines(linebuf);
        }


        // get i/o redirection filename from command line
        static char *getRedirFilename(char *str)
        {
            while(isspace(*str)) str++;
            char *s= str;
            while(*s && !isspace(*s)) s++;
            *s= 0;
            return str;
        }

        // search for operator in wordlist; split command words into words/words2; place operator string in opstring.
        // returns true if an operator was found.
        bool splitByOperator(vector<string> &words/*INOUT*/, string &opstring/*OUT*/, vector<string> &words2/*OUT*/)
        {
            vector<string>::iterator op;
            op= find(words.begin(), words.end(), "&&");
            if(op==words.end()) op= find(words.begin(), words.end(), "&&!");
            if(op!=words.end())
            {
                opstring= *op;
                for(vector<string>::iterator it= op+1; it!=words.end(); ++it)
                    words2.push_back(*it);
                words.erase(op, words.end());
//                words.erase(op);
//                while(op!=words.end())
//                    words2.push_back(*op), words.erase(op);
                return true;
            }
            return false;
        }
};



inline bool CliCommand::readNodeset(FILE *inFile, vector< vector<uint32_t> > &dataset, unsigned expectedSize)
{
    vector<uint32_t> record;
    bool ok= true;
    cliSuccess("\n");
    for(unsigned lineno= 1; ; lineno++)
    {
        record.clear();
        try
        {
            Cli::readNodeIDRecord(inFile, record);
            if(record.empty())
            {
                return ok;
            }
            else if(record.size()!=expectedSize)
            {
                if(ok) cliError(_("error reading data set (line %u)\n"), lineno);
                ok= false;
            }
            else
            {
                // unnecessary leftover, now done in readNodeIDRecord.
    //            if(record[0]==0 || record[1]==0) { cliError(_("invalid node ID in line %d\n"), lineno); ok= false; }
                if(ok) dataset.push_back(record);
            }
        }
        catch(exception& e)
        {
            if(ok) cliError(_("error reading data set (line %u)\n"), lineno);
            ok= false;
        }
    }
}


// check whether a status line or command indicates that a data set will follow. used by core & server.
static inline bool lineIndicatesDataset(const string& line, int *colonPos= 0)
{
    size_t pos= line.rfind(':');
    if(pos==string::npos) return false;
    for(unsigned p= pos+1; p<line.size(); p++)
        if(!isspace(line[p])) return false;
    if(colonPos) *colonPos= pos;
    return true;
}






#endif // CLIBASE_H





