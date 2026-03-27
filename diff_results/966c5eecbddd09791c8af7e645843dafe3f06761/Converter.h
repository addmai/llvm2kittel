--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:17.050788863 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:17.052788779 +0900
@@ -62,7 +62,7 @@
             bool onlyLoopConditions,
             DivRemConstraintType divisionConstraintType, bool bitwiseConditions,
             bool complexityTuples, const bool t2Output, bool signednessInfo,
-            bool nondetTypeInfo, bool unreachableExit);
+            bool nondetTypeInfo, bool unreachableExit, bool ignoreReachError);
   //</Negar>
 
   void phase1(
@@ -336,6 +336,8 @@
   bool
       m_currentBlockReachErrorCalled; // Local: track if called in current block
 
+  const bool m_ignoreReachError; // If true, ignore reach_error() calls
+
 private:
   Converter(const Converter &);
   Converter &operator=(const Converter &);
