import { describe, it, expect } from "vitest";
import { VERBS, verbForTurn } from "../src/verbs";

describe("verbForTurn", () => {
  it("returns the verb at the turn index", () => {
    expect(verbForTurn(0)).toBe(VERBS[0]);
    expect(verbForTurn(3)).toBe(VERBS[3]);
  });
  it("wraps around the list", () => {
    expect(verbForTurn(VERBS.length)).toBe(VERBS[0]);
    expect(verbForTurn(VERBS.length + 2)).toBe(VERBS[2]);
  });
});
