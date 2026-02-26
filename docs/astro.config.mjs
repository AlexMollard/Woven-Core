// @ts-check
import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

// https://astro.build/config
export default defineConfig({
	integrations: [
		starlight({
			title: 'WovenCore Docs',
			sidebar: [
				{
					label: 'Engine Docs',
					autogenerate: { directory: 'engine' },
				},
				{
					label: 'Notes',
					autogenerate: { directory: 'notes' },
				},
				{
					label: 'Vulkan Learning',
					autogenerate: { directory: 'vulkan' },
				},
			],
		}),
	],
});
