--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:17.831756059 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:17.902753077 +0900
@@ -624,9 +624,19 @@
   if (llvm::isa<llvm::ZExtInst>(I)) {
     return getConditionFromValue(I->getOperand(0));
   }
+  // Handle TruncInst (e.g., trunc i8 %c1.0 to i1)
+  if (llvm::isa<llvm::TruncInst>(I)) {
+    if (I->getType() == m_boolType) {
+      return Atom::create(getPolynomial(I), Polynomial::null, Atom::Neq);
+    }
+  }
   if (I->getType() != m_boolType) {
     return Atom::create(getPolynomial(I), Polynomial::null, Atom::Neq);
   }
+  // Handle PHI nodes with bool type
+  if (llvm::isa<llvm::PHINode>(I)) {
+    return Atom::create(getPolynomial(I), Polynomial::null, Atom::Neq);
+  }
   if (llvm::isa<llvm::CmpInst>(I)) {
     llvm::CmpInst *cmp = llvm::cast<llvm::CmpInst>(I);
     if (cmp->getOperand(0)->getType()->isPointerTy()) {
@@ -2822,15 +2832,8 @@
 }
 
 void Converter::visitPHINode(llvm::PHINode &I) {
-  if (I.getType() == m_boolType || !I.getType()->isIntegerTy()) {
-    //<Negar>
-    // if (!m_phase1) {
-    //    std::string phiVarName = I.getName().str();
-    //    std::cout << "v" << phiVarName << " := " << "var__temp_v" + phiVarName
-    //    << ";" << std::endl;
-    //}
-    //</Negar>
-
+  // Skip non-integer types (but process bool as i1)
+  if (!I.getType()->isIntegerTy()) {
     return;
   }
   std::string phiVar = getVar(&I);
@@ -3694,7 +3697,8 @@
 }
 
 void Converter::visitTruncInst(llvm::TruncInst &I) {
-  if (I.getType() == m_boolType) {
+  // Skip non-integer types (but process bool as i1)
+  if (!I.getType()->isIntegerTy()) {
     return;
   }
   if (m_phase1) {
