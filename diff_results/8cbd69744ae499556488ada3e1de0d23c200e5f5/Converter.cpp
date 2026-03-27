--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:16.697803690 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:16.699803606 +0900
@@ -107,7 +107,8 @@
       m_loopConditionBlocks(), m_divisionConstraintType(divisionConstraintType),
       m_bitwiseConditions(bitwiseConditions),
       m_complexityTuples(complexityTuples), m_complexityLHSs(),
-      m_t2Output(t2Output), signednessInfo(signednessInfo),
+      m_t2Output(t2Output), m_reachErrorCalled(false),
+      m_currentBlockReachErrorCalled(false), signednessInfo(signednessInfo),
       nondetTypeInfo(nondetTypeInfo), unreachableExit(unreachableExit) {}
 //</Negar>
 
@@ -353,7 +354,7 @@
   }
 
   // Output assert_error block if reach_error was called
-  if (reachErrorCalled && m_t2Output) {
+  if (m_reachErrorCalled && m_t2Output) {
     std::cout << "FROM: assert_error;" << std::endl;
     std::cout << "TO: assert_error;" << std::endl;
     std::cout << std::endl;
@@ -945,6 +946,9 @@
 }
 
 void Converter::visitBB(llvm::BasicBlock *bb) {
+  // Reset local flag for current basic block
+  m_currentBlockReachErrorCalled = false;
+
   // start
   if (bb == m_entryBlock) {
     ref<Term> lhs = Term::create(getEval(m_function, "start"), m_lhs);
@@ -1130,17 +1134,20 @@
     // ToDO: Remove the self-loop addition in T2 in these cases.
     if (llvm::isa<llvm::ReturnInst>(I)) {
       llvm::BasicBlock *pBlock = I.getParent();
-      if (m_t2Output) {
+      if (m_t2Output && !m_currentBlockReachErrorCalled) {
         std::string retNode = (pBlock->getName().str()) + "_ret";
         std::cout << "TO: " << retNode << ";" << std::endl;
       }
     } else if (llvm::isa<llvm::UnreachableInst>(I)) {
       //<Negar>
       // std::cout << "TO: 1;" << std::endl;
-      if (unreachableExit) {
-        std::cout << "TO: " + lastBasicBlockName + "_ret;" << std::endl;
-      } else {
-        std::cout << "TO: " + I.getParent()->getName().str() + ";" << std::endl;
+      if (m_t2Output && !m_currentBlockReachErrorCalled) {
+        if (unreachableExit) {
+          std::cout << "TO: " + lastBasicBlockName + "_ret;" << std::endl;
+        } else {
+          std::cout << "TO: " + I.getParent()->getName().str() + ";"
+                    << std::endl;
+        }
       }
       //</Negar>
     } else {
@@ -1217,7 +1224,7 @@
         // Later when visiting terminating transition
         // will print transition from true to BB
         llvm::BasicBlock *iBlock = branch->getSuccessor(0);
-        if (m_t2Output && !reachErrorCalled) {
+        if (m_t2Output && !m_currentBlockReachErrorCalled) {
           std::cout << "TO: " << (iBlock->getName().str()) << ";" << std::endl;
         }
       } else {
@@ -1233,7 +1240,7 @@
           exit(1);
         }
 
-        if (m_t2Output && !reachErrorCalled) {
+        if (m_t2Output && !m_currentBlockReachErrorCalled) {
           // Given then we're branching, create an intermediate node to branch
           // through via condition.
           std::cout << "TO: " << (pBlock->getName().str()) << "_end;"
@@ -2593,7 +2600,8 @@
               // Create a transition to assert_error block
               std::cout << "TO: assert_error;" << std::endl;
               std::cout << std::endl;
-              reachErrorCalled = true;
+              m_currentBlockReachErrorCalled = true; // For current block
+              m_reachErrorCalled = true;             // For global tracking
             }
             // Continue with normal non-returning behavior
           }
