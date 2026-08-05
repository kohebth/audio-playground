export const UNIT_DRAG_TYPE = 'application/x-apg-unit';
export const PROJECT_INSTANCE_DRAG_TYPE = 'application/x-apg-project-instance';

export type ProjectLibraryPointerDrag = {
  phase: 'dragging' | 'drop' | 'cancel';
  unitId: string;
  title: string;
  clientX: number;
  clientY: number;
};
