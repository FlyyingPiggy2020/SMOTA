<script setup>
import { NScrollbar } from 'naive-ui'
import SerialConfig from './SerialConfig.vue'
import TcpConfig from './TcpConfig.vue'
import SettingConfig from './SettingConfig.vue'
import AboutConfig from './AboutConfig.vue'

defineProps({
    panel: String
})
</script>

<template>
    <n-scrollbar>
        <SerialConfig v-if="panel === 'serial'" />
        <TcpConfig v-else-if="panel === 'tcp'" />
        <SettingConfig v-else-if="panel === 'setting'" />
        <AboutConfig v-else-if="panel === 'info'" />
    </n-scrollbar>
</template>
