popq %rax
nop
ret
movq %rax, %rdi
ret              # ret to call `touch2`
