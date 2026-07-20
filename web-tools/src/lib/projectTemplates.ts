import type { WorkspaceFile } from './backendSamples';
import { createApgProjectPackageFromFiles, type ApgProjectPackage, type StudioMode } from './projectPackage.ts';

export function projectSlug(name: string): string {
  const slug = name.toLowerCase().replace(/[^a-z0-9]+/g, '_').replace(/^_+|_+$/g, '');
  return /^[a-z]/.test(slug) ? slug : `project_${slug || 'untitled'}`;
}

export function createEmptyProjectYaml(name: string): string {
  const slug = projectSlug(name);
  return `kind: apg.project
schema: apg.project.v2
name: ${slug}
version: 2.0.0

units: []

chain:
  nodes: []
  routes:
    - from: system.input
      to: system.output

targets:
  default: desktop_full
  export:
    - wasm_realtime
    - offline_render
`;
}

export function createEmptyProjectPackage(options: {
  id: string;
  name: string;
  mode?: StudioMode;
  now?: string;
}): ApgProjectPackage {
  const slug = projectSlug(options.name);
  const path = `projects/${slug}.project.v2.yaml`;
  const content = createEmptyProjectYaml(options.name);
  const files: WorkspaceFile[] = [{ path, role: 'project', content, originalContent: content }];
  return createApgProjectPackageFromFiles(path, files, {
    id: options.id,
    name: options.name.trim() || 'Untitled Project',
    mode: options.mode,
    createdAt: options.now,
    updatedAt: options.now,
  });
}
