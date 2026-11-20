
// Generated from Cypher.g4 by ANTLR 4.13.1

#pragma once


#include "antlr4-runtime.h"




class  CypherLexer : public antlr4::Lexer {
public:
  enum {
    T__0 = 1, T__1 = 2, T__2 = 3, T__3 = 4, T__4 = 5, T__5 = 6, T__6 = 7, 
    T__7 = 8, T__8 = 9, T__9 = 10, T__10 = 11, T__11 = 12, T__12 = 13, T__13 = 14, 
    T__14 = 15, T__15 = 16, T__16 = 17, T__17 = 18, T__18 = 19, T__19 = 20, 
    T__20 = 21, T__21 = 22, T__22 = 23, T__23 = 24, T__24 = 25, T__25 = 26, 
    T__26 = 27, T__27 = 28, T__28 = 29, T__29 = 30, T__30 = 31, T__31 = 32, 
    T__32 = 33, T__33 = 34, T__34 = 35, T__35 = 36, T__36 = 37, T__37 = 38, 
    T__38 = 39, T__39 = 40, T__40 = 41, T__41 = 42, T__42 = 43, T__43 = 44, 
    ACYCLIC = 45, ANY = 46, ADD = 47, ALL = 48, ALTER = 49, ANALYZE = 50, 
    AND = 51, AS = 52, ASSERT = 53, ASC = 54, ASCENDING = 55, ATTACH = 56, 
    BEGIN = 57, BY = 58, CALL = 59, CASE = 60, CAST = 61, CHECKPOINT = 62, 
    COLUMN = 63, COMMENT = 64, COMMIT = 65, COMMIT_SKIP_CHECKPOINT = 66, 
    CONTAINS = 67, COPY = 68, COUNT = 69, CREATE = 70, CSR = 71, CYCLE = 72, 
    DATABASE = 73, DBTYPE = 74, DEFAULT = 75, DELETE = 76, DESC = 77, DESCENDING = 78, 
    DETACH = 79, DISTINCT = 80, DROP = 81, ELSE = 82, END = 83, ENDS = 84, 
    EXISTS = 85, EXPLAIN = 86, EXPORT = 87, EXTENSION = 88, FALSE = 89, 
    FROM = 90, FORCE = 91, FOR = 92, GLOB = 93, GRAPH = 94, GROUP = 95, 
    HASH = 96, HEADERS = 97, HINT = 98, IMPORT = 99, INDEX = 100, IF = 101, 
    IN = 102, INCREMENT = 103, INSTALL = 104, IS = 105, JOIN = 106, KEY = 107, 
    LIMIT = 108, LIST = 109, LOAD = 110, LOGICAL = 111, MACRO = 112, MATCH = 113, 
    MAXVALUE = 114, MERGE = 115, MINVALUE = 116, MULTI_JOIN = 117, NO = 118, 
    NODE = 119, NOT = 120, NONE = 121, NULL_ = 122, ON = 123, ONLY = 124, 
    OPTIONS = 125, OPTIONAL = 126, OR = 127, ORDER = 128, PRIMARY = 129, 
    PROFILE = 130, PROJECT = 131, RANGE = 132, READ = 133, REL = 134, RENAME = 135, 
    RETURN = 136, ROLLBACK = 137, ROLLBACK_SKIP_CHECKPOINT = 138, SEQUENCE = 139, 
    SET = 140, SORTED = 141, SHORTEST = 142, START = 143, STARTS = 144, 
    STRUCT = 145, TABLE = 146, THEN = 147, TO = 148, TRAIL = 149, TRANSACTION = 150, 
    TRUE = 151, TYPE = 152, UNION = 153, UNWIND = 154, UNINSTALL = 155, 
    UPDATE = 156, USE = 157, WHEN = 158, WHERE = 159, WITH = 160, WRITE = 161, 
    WSHORTEST = 162, XOR = 163, SINGLE = 164, YIELD = 165, USER = 166, PARTITION = 167, 
    PARTITIONS = 168, PASSWORD = 169, ROLE = 170, MAP = 171, NAMESPACE = 172, 
    NAMESPACES = 173, GLOBAL = 174, LOCAL = 175, REMOVE = 176, DECIMAL = 177, 
    STAR = 178, L_SKIP = 179, INVALID_NOT_EQUAL = 180, COLON = 181, DOTDOT = 182, 
    MINUS = 183, FACTORIAL = 184, StringLiteral = 185, EscapedChar = 186, 
    DecimalInteger = 187, HexLetter = 188, HexDigit = 189, Digit = 190, 
    NonZeroDigit = 191, NonZeroOctDigit = 192, ZeroDigit = 193, ExponentDecimalReal = 194, 
    RegularDecimalReal = 195, UnescapedSymbolicName = 196, IdentifierStart = 197, 
    IdentifierPart = 198, EscapedSymbolicName = 199, SP = 200, WHITESPACE = 201, 
    CypherComment = 202, Unknown = 203
  };

  explicit CypherLexer(antlr4::CharStream *input);

  ~CypherLexer() override;


  std::string getGrammarFileName() const override;

  const std::vector<std::string>& getRuleNames() const override;

  const std::vector<std::string>& getChannelNames() const override;

  const std::vector<std::string>& getModeNames() const override;

  const antlr4::dfa::Vocabulary& getVocabulary() const override;

  antlr4::atn::SerializedATNView getSerializedATN() const override;

  const antlr4::atn::ATN& getATN() const override;

  // By default the static state used to implement the lexer is lazily initialized during the first
  // call to the constructor. You can call this function if you wish to initialize the static state
  // ahead of time.
  static void initialize();

private:

  // Individual action functions triggered by action() above.

  // Individual semantic predicate functions triggered by sempred() above.

};

