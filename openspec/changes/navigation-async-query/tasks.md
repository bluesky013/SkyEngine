## 1. Query API

- [ ] 1.1 Define `NaviPath` result (points + flags) and expose it from the query
- [ ] 1.2 Add a `NaviQueryFilter` creation path (factory on `NaviMesh`/registry)
- [ ] 1.3 Keep a synchronous query variant for tools/tests

## 2. Asynchronous query

- [ ] 2.1 Add an async path request (submit + handle + callback/poll)
- [ ] 2.2 Run queries on worker threads via a `dtNavMeshQuery` pool
- [ ] 2.3 Cancellation and in-flight invalidation on nav mesh change

## 3. Budgeting

- [ ] 3.1 Budget/queue async queries per frame
- [ ] 3.2 Verify no main-thread stall under many concurrent requests

## 4. Tests

- [ ] 4.1 Headless test: async query returns the expected path on a built tile
- [ ] 4.2 Verify cancellation and mesh-change invalidation

## 5. Related backlog

- [ ] 5.1 `navigation-agents`: DetourCrowd agents consuming async queries (split into its own change)
