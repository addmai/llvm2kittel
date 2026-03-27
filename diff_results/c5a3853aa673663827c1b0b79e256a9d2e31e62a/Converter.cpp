--- /tmp/tmp.QMgVR8dB4J/old_file.cpp	2026-03-27 17:06:17.443772356 +0900
+++ /tmp/tmp.QMgVR8dB4J/new_file.cpp	2026-03-27 17:06:17.589766224 +0900
@@ -453,7 +453,8 @@
 std::string Converter::getVar(llvm::Value *V) {
   std::string name = V->getName();
   // Remove dots from variable names to avoid syntax errors in T2 format
-  name.erase(std::remove(name.begin(), name.end(), '.'), name.end());
+  // COMMENTED OUT: Let dots remain in variable names
+  // name.erase(std::remove(name.begin(), name.end(), '.'), name.end());
   if (!name.empty() && name[0] == '\'') {
     name = name.substr(1);
   } else {
@@ -2573,10 +2574,11 @@
           std::string leftVar = I.getOperand(0)->getName().str();
           std::string rightVar = I.getOperand(1)->getName().str();
           // Remove dots from variable names to avoid syntax errors in T2 format
-          leftVar.erase(std::remove(leftVar.begin(), leftVar.end(), '.'),
-                        leftVar.end());
-          rightVar.erase(std::remove(rightVar.begin(), rightVar.end(), '.'),
-                         rightVar.end());
+          // COMMENTED OUT: Let dots remain
+          // leftVar.erase(std::remove(leftVar.begin(), leftVar.end(), '.'),
+          //               leftVar.end());
+          // rightVar.erase(std::remove(rightVar.begin(), rightVar.end(), '.'),
+          //                rightVar.end());
           std::cout << "v" << leftVar << " := v" << rightVar << ";"
                     << std::endl;
         }
@@ -2870,14 +2872,20 @@
       // Variable-index
       std::string arrayName = gepInst.getPointerOperand()->getName().str();
       // Remove dots from array names
-      arrayName.erase(std::remove(arrayName.begin(), arrayName.end(), '.'),
-                      arrayName.end());
-      if (arrayName.find("str") == std::string::npos &&
-          arrayName.find("__PRETTY_FUNCTION__") == std::string::npos) {
+      // COMMENTED OUT: Let dots remain for consistency
+      // arrayName.erase(std::remove(arrayName.begin(), arrayName.end(), '.'),
+      //                 arrayName.end());
+      // Exclude string literals (.str, .str.N) and __PRETTY_FUNCTION__ but
+      // allow user arrays like string_A
+      bool isStringLiteral =
+          (arrayName.find(".str") != std::string::npos) ||
+          (arrayName.find("__PRETTY_FUNCTION__") != std::string::npos);
+      if (!isStringLiteral) {
         std::string varName = gepInst.getName().str();
         // Remove dots from variable names
-        varName.erase(std::remove(varName.begin(), varName.end(), '.'),
-                      varName.end());
+        // COMMENTED OUT: Let dots remain for consistency
+        // varName.erase(std::remove(varName.begin(), varName.end(), '.'),
+        //               varName.end());
         llvm::Value *arrayIndexOperand =
             gepInst.getOperand(gepInst.getNumOperands() - 1);
         std::string index;
@@ -2889,8 +2897,10 @@
           // Variable-index
           std::string indexName = arrayIndexOperand->getName().str();
           // Remove dots from index variable names
-          indexName.erase(std::remove(indexName.begin(), indexName.end(), '.'),
-                          indexName.end());
+          // COMMENTED OUT: Let dots remain for consistency
+          // indexName.erase(std::remove(indexName.begin(), indexName.end(),
+          // '.'),
+          //                 indexName.end());
           index = "v" + indexName;
         }
         llvm::Type *elementType =
@@ -2912,11 +2922,12 @@
         std::string structInstance =
             gepInst.getPointerOperand()->getName().str();
         // Remove dots from variable names to avoid syntax errors in T2 format
-        varName.erase(std::remove(varName.begin(), varName.end(), '.'),
-                      varName.end());
-        structInstance.erase(
-            std::remove(structInstance.begin(), structInstance.end(), '.'),
-            structInstance.end());
+        // COMMENTED OUT: Let dots remain for consistency
+        // varName.erase(std::remove(varName.begin(), varName.end(), '.'),
+        //               varName.end());
+        // structInstance.erase(
+        //     std::remove(structInstance.begin(), structInstance.end(), '.'),
+        //     structInstance.end());
         llvm::Value *accessedField =
             gepInst.getOperand(gepInst.getNumOperands() - 1);
         int accessedFieldId =
@@ -2929,16 +2940,18 @@
         //<second>
         std::string basePoint = gepInst.getPointerOperand()->getName().str();
         // Remove dots
-        basePoint.erase(std::remove(basePoint.begin(), basePoint.end(), '.'),
-                        basePoint.end());
+        // COMMENTED OUT: Let dots remain for consistency
+        // basePoint.erase(std::remove(basePoint.begin(), basePoint.end(), '.'),
+        //                 basePoint.end());
         if (std::find(oneDimArrs.begin(), oneDimArrs.end(), "v" + basePoint) !=
             oneDimArrs.end()) {
           // Access an element of an array -> One-dimensional -> Flexible access
           // -> 1 (struct)
           std::string varName = gepInst.getName().str();
           // Remove dots
-          varName.erase(std::remove(varName.begin(), varName.end(), '.'),
-                        varName.end());
+          // COMMENTED OUT: Let dots remain for consistency
+          // varName.erase(std::remove(varName.begin(), varName.end(), '.'),
+          //               varName.end());
           llvm::Value *arrayIndexOperand =
               gepInst.getOperand(gepInst.getNumOperands() - 1);
           std::string index;
@@ -2948,9 +2961,10 @@
           } else {
             std::string indexVarName = arrayIndexOperand->getName().str();
             // Remove dots
-            indexVarName.erase(
-                std::remove(indexVarName.begin(), indexVarName.end(), '.'),
-                indexVarName.end());
+            // COMMENTED OUT: Let dots remain
+            // indexVarName.erase(
+            //     std::remove(indexVarName.begin(), indexVarName.end(), '.'),
+            //     indexVarName.end());
             index = "v" + indexVarName;
           }
           std::cout << "v" << varName << " := select_array(v" << basePoint
@@ -3134,8 +3148,9 @@
                               });
       if (it1 != arrayInsts.end()) {
         newArg = Polynomial::create("nondef_nf");
+        // Use the actual index from arrayInsts, not the pointer variable
         std::cout << destinationVar << " := " << "select_array(" << it1->array
-                  << ", " << sourceVar << ");" << std::endl;
+                  << ", " << it1->index << ");" << std::endl;
       }
 
       else {
@@ -3199,13 +3214,15 @@
 
       if (std::find(oneDimArrs.begin(), oneDimArrs.end(), arrayName) !=
           oneDimArrs.end()) {
+        // Use the actual index from arrayInsts, not the pointer variable
         std::cout << arrayName << " := store_array(" << arrayName << ", "
-                  << destinationVar << ", " << sourceVar << ");" << std::endl;
+                  << it1->index << ", " << sourceVar << ");" << std::endl;
       }
 
       else {
+        // Use the actual index from arrayInsts, not the pointer variable
         std::cout << arrayName << " := store_array(" << arrayName << ", "
-                  << destinationVar << ", " << sourceVar << ");" << std::endl;
+                  << it1->index << ", " << sourceVar << ");" << std::endl;
         auto it2 = std::find_if(arrayInsts.begin(), arrayInsts.end(),
                                 [&arrayName](const ArrayInst &arrayInst) {
                                   return arrayInst.variable == arrayName;
@@ -3307,6 +3324,7 @@
     if (allocatedType->isArrayTy()) {
       // Define an array -> Locally -> One-dimensional -> Fixed-sized
       std::string arrayName = "v" + allocaInst.getName().str();
+      // Initialize array as nondet_array for proper array theory support
       std::cout << arrayName << " := nondet();" << std::endl;
       if (std::find(oneDimArrs.begin(), oneDimArrs.end(), arrayName) ==
           oneDimArrs.end()) {
@@ -3670,10 +3688,11 @@
     std::string leftVar = I.getName().str();
     std::string rightVar = I.getOperand(0)->getName().str();
     // Remove dots from variable names to avoid syntax errors in T2 format
-    leftVar.erase(std::remove(leftVar.begin(), leftVar.end(), '.'),
-                  leftVar.end());
-    rightVar.erase(std::remove(rightVar.begin(), rightVar.end(), '.'),
-                   rightVar.end());
+    // COMMENTED OUT: Let dots remain
+    // leftVar.erase(std::remove(leftVar.begin(), leftVar.end(), '.'),
+    //               leftVar.end());
+    // rightVar.erase(std::remove(rightVar.begin(), rightVar.end(), '.'),
+    //                rightVar.end());
     if (signednessInfo) {
       unsigned destinationBits = I.getType()->getIntegerBitWidth();
       std::cout << "v" << leftVar << " := extract(" << (destinationBits - 1)
