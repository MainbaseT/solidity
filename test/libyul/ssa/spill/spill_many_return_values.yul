{
    mstore(0x40, memoryguard(0x120))
    function many() -> r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15, r16, r17 {}
    let v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17 := many()
    sstore(v0, v1)
    sstore(v2, v3)
    sstore(v4, v5)
    sstore(v6, v7)
    sstore(v8, v9)
    sstore(v10, v11)
    sstore(v12, v13)
    sstore(v14, v15)
    sstore(v16, v17)
}
// ----
