
#pragma once

#include <task.h>
#include <vma.h>


void remove_vma(struct task *task, struct vma *vma);
void free_all_vmas(struct task *task);
int unmap_and_remove_vma_range(struct task *task, void *base, size_t size);
int unmap_clean_pages_range(struct task *task, void *base, size_t size);
