<script setup>
import { ref } from 'vue'
import { NMenu } from 'naive-ui'

const menuOptions = [
    { label: '菜单', key: 'menu' },
    { label: '设置', key: 'settings' },
    { label: '软件信息', key: 'info' },
    { label: '关于作者', key: 'about' },
    { label: '预留', key: '1' },
    { label: '预留测试', key: '2' },
    { label: '预留', key: '3' },
    { label: '预留', key: '4' }
]

const activeKey = ref('menu')
</script>

<template>
    <n-menu
        v-model:value="activeKey"
        mode="horizontal"
        :options="menuOptions"
        responsive
    />
</template>
