--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:17.343776556 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:17.331777060 +0900
@@ -15,7 +15,7 @@
       m_currFunName(""), m_currBB(NULL), m_namedGlobals(false) {}
 
 static bool filter(char c) {
-  return (c == ' ') || (c == '*') || (c == '+') || (c == '-');
+  return (c == ' ') || (c == '*') || (c == '+') || (c == '-') || (c == '.');
 }
 
 static std::string sanitize(std::string s) {
