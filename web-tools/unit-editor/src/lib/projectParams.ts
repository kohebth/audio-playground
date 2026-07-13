import type { ProjectInspect, ProjectInstance } from './backendSamples';

export type ParamDrafts = Record<string, string>;
export type ParamOriginals = Record<string, string>;

export type ParamOverride = {
  path: string;
  instanceId: string;
  key: string;
  value: string;
  originalValue: string;
};

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

export const buildParamOriginals = buildParamDrafts;

export function countDirtyParams(project: ProjectInspect, drafts: ParamDrafts): number {
  return project.nodes.reduce((count, instance) => count + countDirtyParamsForInstance(instance, drafts), 0);
}

export function countDirtyParamsForInstance(instance: ProjectInstance, drafts: ParamDrafts): number {
  return instance.params.filter(param => (drafts[paramDraftKey(instance.id, param.key)] ?? param.value) !== param.value)
    .length;
}

export function buildParamOverrides(project: ProjectInspect, drafts: ParamDrafts): ParamOverride[] {
  return buildParamOverridesFromOriginals(project, drafts, buildParamOriginals(project));
}

export function buildParamOverridesFromOriginals(
  project: ProjectInspect,
  drafts: ParamDrafts,
  originals: ParamOriginals,
): ParamOverride[] {
  return project.nodes.flatMap(instance =>
    instance.params.flatMap(param => {
      const value = drafts[paramDraftKey(instance.id, param.key)] ?? param.value;
      const originalValue = originals[paramDraftKey(instance.id, param.key)] ?? param.value;
      if (value === originalValue) return [];

      return [{
        path: `${instance.id}.${param.key}`,
        instanceId: instance.id,
        key: param.key,
        value,
        originalValue,
      }];
    }),
  );
}
