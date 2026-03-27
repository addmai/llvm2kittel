--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:17.301778320 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:17.321777480 +0900
@@ -177,7 +177,12 @@
   return ""; // Can always chosen to be true; no need to output (assert true)
 }
 
-std::string Nondef::toT2String() { return "nondet()"; }
+std::string Nondef::toT2String() {
+  // Return a valid condition for assume statements
+  // Instead of just "nondet()", use "nondet() != 0" to make it a valid
+  // condition
+  return "nondet() != 0";
+}
 
 ref<Constraint> Nondef::instantiate(std::map<std::string, ref<Polynomial>> *) {
   return new Nondef();
