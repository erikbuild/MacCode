// ABOUTME: Local gerund list for the MacCode ✻ verb line.
// ABOUTME: The Agent SDK doesn't surface Claude Code's flavor verbs, so the proxy supplies them.
export const VERBS = [
  "Forging", "Cogitating", "Pondering", "Herding", "Conjuring",
  "Tinkering", "Noodling", "Synthesizing", "Untangling", "Brewing",
];

export function verbForTurn(turnIndex: number): string {
  return VERBS[turnIndex % VERBS.length];
}
