--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:16.420815325 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:16.399816207 +0900
@@ -127,6 +127,7 @@
   //<Negar>
   bool isEntryBlock = true;
   // bool hasUnreachableBlock = false;
+  bool reachErrorCalled = false;
   std::string lastBasicBlockName;
   std::vector<std::string> mulInsts;
   struct PhiInst {
