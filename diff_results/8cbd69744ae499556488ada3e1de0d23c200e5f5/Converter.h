--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:16.649805706 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:16.651805622 +0900
@@ -332,6 +332,9 @@
   std::set<std::string> m_complexityLHSs;
 
   const bool m_t2Output;
+  bool m_reachErrorCalled; // Global: track if reach_error() was called in phase
+  bool
+      m_currentBlockReachErrorCalled; // Local: track if called in current block
 
 private:
   Converter(const Converter &);
