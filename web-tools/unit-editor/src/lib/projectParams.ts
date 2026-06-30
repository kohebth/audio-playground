import type { ProjectInspect, ProjectInstance } from './backendSamples';

export type ParamDrafts = Record<string, string>;

export function paramDraftKey(instanceId: string, paramKey: string): string {
  return `${instanceId}.${paramKey}`;
}

export function buildParamDrafts(project: ProjectInspect): ParamDrafts {
  const drafts: ParamDrafts = {};

  for (const instance of project.nodes) {
    for (const param of instance.params) {
      drafts[paramDraftKey(instance.id, param.key)] = param.value;
    }
  }

  return drafts;
}

export function countDirtyParams(project: ProjectInspect, drafts: ParamDrafts): number {
  return project.nodes.reduce((count, instance) => count + countDirtyParamsForInstance(instance, drafts), 0);
}

export function countDirtyParamsForInstance(instance: ProjectInstance, drafts: ParamDrafts): number {
  return instance.params.filter(param => (drafts[paramDraftKey(instance.id, param.key)] ?? param.value) !== param.value)
    .length;
}
