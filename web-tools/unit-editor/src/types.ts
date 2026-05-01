// Core data model for a .unit.yaml file

export type ParamDef = {
  name: string;
  default: number;
  range?: [number, number];
  comment?: string;
};

export type KV = { key: string; value: string };  // value may be "${params.x}" or literal

export type Stage = {
  id: string;
  fn: string;
  in: KV[];
  out: KV[];
  config: KV[];
};

export type UnitConfig = {
  name: string;
  version: string;
  params: ParamDef[];
  signals: string[];
  pipeline: Stage[];
};
