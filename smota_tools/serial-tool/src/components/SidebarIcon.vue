<script setup>
import { NScrollbar } from 'naive-ui'

const props = defineProps({
    active: String
})

const emit = defineEmits(['update:active'])

const icons = [
    { key: 'serial', icon: '🔌' },
    { key: 'tcp', icon: '🌐' },
    { key: 'setting', icon: '⚙️' },
    { key: 'info', icon: 'ℹ️' },
]
</script>

<template>
    <n-scrollbar>
        <div class="flex flex-col items-center py-2">
            <div
                v-for="item in icons"
                :key="item.key"
                class="w-16 h-16 flex items-center justify-center text-2xl cursor-pointer hover:bg-gray-200 rounded transition-colors"
                @click="emit('update:active', item.key)"
            >
                {{ item.icon }}
            </div>
        </div>
    </n-scrollbar>
</template>
