#pragma once
/*
 * This container template is written by Vladislav Gelfer. The original
 * article can be viewed here http://www.codeproject.com/KB/recipes/Containers.aspx
 */
// Disable level-4 warnings. We've examined them and found non-harmful.
// Compile this file at level 3.
#pragma warning (push, 3)

namespace MetalBone  {
namespace ThirdParty {
	template <class T>
	inline void swap_t(T& var1, T& var2)
	{
		T var = var1;
		var1 = var2;
		var2 = var;
	}

	template <class ARG_KEY>
	inline size_t MapInlHashKey(ARG_KEY nKey) { return (size_t) nKey; }

	template <class ARG_KEY>
	struct TreeInlComparator {
		ARG_KEY m_Key;
		TreeInlComparator(ARG_KEY key) : m_Key(key) {}
		int Compare(ARG_KEY key) { return (key < m_Key) ? (-1) : (key > m_Key) ? 1 : 0; }
	};

	// The following comparator should be used to compare long strings
	template <class T>
	struct TreeInlComparator_String {

		const T* m_szTxt;
		ULONG m_nLMatch;
		ULONG m_nRMatch;

		TreeInlComparator_String(const T* txt) : m_szTxt(txt), m_nLMatch(0), m_nRMatch(0){}
		int Compare(const T* szVal)
		{
			for (ULONG nPos = min(m_nLMatch, m_nRMatch); ; nPos++)
			{
				T chThis = m_szTxt[nPos];
				T chArg = szVal[nPos];
				if (chArg < chThis)
				{
					m_nLMatch = nPos;
					return -1;
				}
				if (chArg > chThis)
				{
					m_nRMatch = nPos;
					return 1;
				}
				if (!chThis)
					return 0;
			}
		}
	};



	namespace Container
	{
		namespace Impl
		{
			// Helper implementation template classes.

			// Counter that inherits any engine.
			template <class Eng, class T_COUNT>
			class InhCounter : public Eng
			{
				T_COUNT m_nCount;
			protected:
				InhCounter() :m_nCount(0) {}

				void zInc()                          { m_nCount++; }
				void zDec()                          { M_ASSERT(m_nCount > 0); m_nCount--; }
				void zResetCount()                   { m_nCount = 0; }
				void zAssertCount(size_t nVal) const { M_ASSERT(m_nCount == (T_COUNT) nVal); }
				void zSwapCount(InhCounter& other)   { swap_t(m_nCount, other.m_nCount); }

			public:
				typename T_COUNT GetCount() const    { return m_nCount; }
				__declspec(property(get=GetCount)) T_COUNT _Count;
			};

			template <class Eng>
			class InhCounter<Eng, void> : public Eng
			{
			protected:
				void zInc()                          {}
				void zDec()                          {}
				void zResetCount()                   {}
				void zAssertCount(size_t nVal) const {}
				void zSwapCount(InhCounter& other)   {}
			};

			// Extension to any container that creates/deletes elements dynamically.
			template <class Eng, class CastNode>
			class InhDyn : public Eng
			{
			public:
				~InhDyn() { Clear(); }

				// The following methods demand to have delete for CastNode.
				void Clear()
				{
					while (!_Empty)
						delete (CastNode*) zAnyOrderRemove();
				}

				// Must also support random remove.
				void Delete(CastNode* pNode)
				{
					Remove(*pNode);
					delete pNode;
				}
			};


			// Linked list base node. May come with either Prev, Next or both pointers.
			template <bool Prev, bool Next> class Node;
			template <> class Node<true, false> { protected: Node* m_pPrev; template <bool Prev, bool Next, bool Head, bool Tail> friend class ListEngBase; };
			template <> class Node<false, true> { protected: Node* m_pNext; template <bool Prev, bool Next, bool Head, bool Tail> friend class ListEngBase; };
			template <> class Node<true, true>  { protected: Node* m_pPrev; Node* m_pNext; template <bool Prev, bool Next, bool Head, bool Tail> friend class ListEngBase; };

			// Base container of either Head, Tail or both pointers.
			template <class Node, bool Head, bool Tail> struct InhHT;
			template <class Node> struct InhHT<Node, true, false> { Node* m_pHead; };
			template <class Node> struct InhHT<Node, false, true> { Node* m_pTail; };
			template <class Node> struct InhHT<Node, true, true> : public InhHT<Node, true, false> , public InhHT<Node, false, true> {};

			// Base linked list engine.
			template <bool Prev, bool Next, bool Head, bool Tail>
			class ListEngBase : protected InhHT<Node<Prev, Next>, Head, Tail>
			{
			public:
				typedef Node<Prev, Next> Node;

				bool IsEmpty() const { return zIsEmpty<Head>(); }
				__declspec(property(get=IsEmpty)) bool _Empty;

			protected:
				ListEngBase() { Reset(); }
				~ListEngBase() { M_ASSERT(_Empty); }

				void Reset()
				{
					zSetHead<true>(NULL);
					zSetHead<false>(NULL);
				}
				void Swap(ListEngBase& lst)
				{
					zSwapHead<Head>(lst);
					zSwapTail<Tail>(lst);
				}
				void Remove(Node& node)
				{
					zRemove<true>(node);
					zRemove<false>(node);
				}

#ifdef _DEBUG
				size_t AssertValid() const
				{
					return zAssertValid<Head && Next>();
				}
#endif // _DEBUG
			private:
				// Empty test
				template <bool Head> bool zIsEmpty() const;
				template <> bool zIsEmpty<true>()  const { return !m_pHead; }
				template <> bool zIsEmpty<false>() const { return !m_pTail; }

				//
				// The following template functions operate on vise/versa parameter.
				//

				// Head/Tail retrive
				template <bool vise> Node* zGetHead();
				template <> Node* zGetHead<true>()  { return m_pHead; }
				template <> Node* zGetHead<false>() { return m_pTail; }
				// Head/Tail set
				template <bool vise> void zSetHead(Node* pVal);
				template <> void zSetHead<true>(Node* pVal)    { zSetHeadEx<Head>(pVal); }
				template <> void zSetHead<false>(Node* pVal)   { zSetTailEx<Tail>(pVal); }
				template <bool> void zSetHeadEx(Node* pVal)    { m_pHead = pVal; }
				template <bool> void zSetTailEx(Node* pVal)    { m_pTail = pVal; }
				template <> void zSetHeadEx<false>(Node* pVal) {}
				template <> void zSetTailEx<false>(Node* pVal) {}
				// Node prev/next retrieve
				template <bool vise> static Node* zGetNext(Node&);
				template <> static Node* zGetNext<true>(Node& node)  { return node.m_pNext; }
				template <> static Node* zGetNext<false>(Node& node) { return node.m_pPrev; }
				// Node prev/next set
				template <bool vise> static void zSetNext(Node&, Node* pVal);
				template <> static void zSetNext<true>(Node& node, Node* pVal)    { zSetNextEx<Next>(node, pVal); }
				template <> static void zSetNext<false>(Node& node, Node* pVal)   { zSetPrevEx<Prev>(node, pVal); }
				template <bool> static void zSetNextEx(Node& node, Node* pVal)    { node.m_pNext = pVal; }
				template <bool> static void zSetPrevEx(Node& node, Node* pVal)    { node.m_pPrev = pVal; }
				template <> static void zSetNextEx<false>(Node& node, Node* pVal) {}
				template <> static void zSetPrevEx<false>(Node& node, Node* pVal) {}
				// Node prev/next assert
#ifdef _DEBUG
				template <bool vise> static void zAssertNext(const Node&, const Node* pVal);
				template <> static void zAssertNext<true>(const Node& node, const Node* pVal)    { zAssertNextEx<Next>(node, pVal); }
				template <> static void zAssertNext<false>(const Node& node, const Node* pVal)   { zAssertPrevEx<Prev>(node, pVal); }
				template <bool> static void zAssertNextEx(const Node& node, const Node* pVal)    { M_ASSERT(node.m_pNext == pVal); }
				template <bool> static void zAssertPrevEx(const Node& node, const Node* pVal)    { M_ASSERT(node.m_pPrev == pVal); }
				template <> static void zAssertNextEx<false>(const Node& node, const Node* pVal) {}
				template <> static void zAssertPrevEx<false>(const Node& node, const Node* pVal) {}
#endif // _DEBUG
				// Swap
				template <bool> void zSwapHead(ListEngBase& lst)    { swap_t(m_pHead, lst.m_pHead); }
				template <bool> void zSwapTail(ListEngBase& lst)    { swap_t(m_pTail, lst.m_pTail); }
				template <> void zSwapHead<false>(ListEngBase& lst) {}
				template <> void zSwapTail<false>(ListEngBase& lst) {}

				template <bool vise> void zInsertNextEx(Node& node, Node* pPos, Node* pLink)
				{
					zSetNext<!vise>(node, pPos);							// node->prev = pos
					zSetNext<vise>(node, pLink);							// node->next = link
					if (pLink)
						zSetNext<!vise>(*pLink, &node);						// link->prev = node
					else
						zSetHead<!vise>(&node);								// tail = node
				}
				template <bool vise> void zRemoveNextEx(Node& node, Node* pPos)
				{
					Node* pLink = zGetNext<vise>(node);						// link = node->next
					if (pLink)
						zSetNext<!vise>(*pLink, pPos);						// link->prev = pos
					else
						zSetHead<!vise>(pPos);								// tail = pos
				}
				template <bool vise> void zRemove(Node& node)
				{
					zRemoveNextEx<vise>(node, zGetNext<!vise>(node));
				}

				// Validity test
#ifdef _DEBUG
				template <bool vise> size_t zAssertValid() const
				{
					size_t nCount = 0;
					for (Node* pVal = ((ListEngBase*) this)->zGetHead<vise>(); pVal; ) // val = head
					{
						if (!nCount)
							zAssertNext<!vise>(*pVal, NULL); // !val->prev

						nCount++;

						Node* pNext = zGetNext<vise>(*pVal); // next = val->next
						if (!pNext)
							break;

						zAssertNext<!vise>(*pNext, pVal); // next->prev == val
						pVal = pNext;
					}
					return nCount;
				}
#endif // _DEBUG

			protected:
				// Inserts
				template <bool vise> void zInsertNext(Node& node, Node& pos)
				{
					zInsertNextEx<vise>(node, &pos, zGetNext<vise>(pos));	// node, pos, pos->next
					zSetNext<vise>(pos, &node);								// pos->next = node
				}
				template <bool vise> void zInsertHead(Node& node)
				{
					zInsertNextEx<vise>(node, NULL, zGetHead<vise>());		// node, NULL, head
					zSetHead<vise>(&node);									// head = node
				}
				template <bool vise> void zInsertNext(Node& node, Node* pPos)
				{
					if (pPos)
						zInsertNext<vise>(node, *pPos);
					else
						zInsertHead<vise>(node);
				}
				// Removes
				template <bool vise> Node* zRemoveNext(Node& pos)
				{
					Node* pNode = zGetNext<vise>(pos); M_ASSERT(pNode);		// node = pos->next
					zSetNext<vise>(pos, zGetNext<vise>(*pNode));			// pos->next = node->next
					zRemoveNextEx<vise>(*pNode, &pos);
					return pNode;
				}
				template <bool vise> Node* zRemoveHead()
				{
					Node* pNode = zGetHead<vise>(); M_ASSERT(pNode);			// node = head
					zSetHead<vise>(zGetNext<vise>(*pNode));					// head = node->next
					zRemoveNextEx<vise>(*pNode, NULL);
					return pNode;
				}
				template <bool vise> Node* zRemoveNext(Node* pPos)
				{
					return pPos ? zRemoveNext<vise>(*pPos) : zRemoveHead<vise>();
				}

				// Any order iteration:
				Node* zAnyOrderRemove() { return zRemoveHead<Head && Next>(); }
			};
		}; // namespace Impl

		//////////////////////////////////////////////////////////////
		// Linked List engine either with or without the InhCounter
		template <bool Prev, bool Next, bool Head, bool Tail, class T_COUNT>
		class ListEng : public Impl::InhCounter<Impl::ListEngBase<Prev, Next, Head, Tail>, T_COUNT>
		{
			typedef Impl::ListEngBase<Prev, Next, Head, Tail> Eng;
		protected:
			// should not be instantiated as-is
			ListEng()  {}
			~ListEng() {}

		public:
			typedef typename Eng::Node Node;

			void Reset()
			{
				Eng::Reset();
				zResetCount();
			}
			void Swap(ListEng& lst)
			{
				Eng::Swap(lst);
				zSwapCount(lst);
			}

			void AssertValid() const
			{
#ifdef _DEBUG
				zAssertCount(Eng::AssertValid());
#endif // _DEBUG
			}

		protected:
			// vise/Versa
			template <bool vise> void zInsertNext(Node& node, Node& pos)  { Eng::zInsertNext<vise>(node, pos); zInc(); }
			template <bool vise> Node* zRemoveNext(Node& pos)             { zDec(); return Eng::zRemoveNext<vise>(pos); }
			template <bool vise> void zInsertHead(Node& node)             { Eng::zInsertHead<vise>(node); zInc(); }
			template <bool vise> Node* zRemoveHead()                      { zDec(); return Eng::zRemoveHead<vise>(); }
			template <bool vise> void zInsertNext(Node& node, Node* pPos) { Eng::zInsertNext<vise>(node, pPos); zInc(); }
			template <bool vise> Node* zRemoveNext(Node* pPos)            { zDec(); return Eng::zRemoveNext<vise>(pPos); }
			Node* zAnyOrderRemove() { zDec(); return Eng::zAnyOrderRemove(); }

			void zRemove(Node& node)
			{
				Eng::Remove(node);
				zDec();
			}
		};

		// Standard list. Enherits the engine, plus the cast.
		template <class Eng, class CastNode = Eng::Node>
		class ListEx : public Eng
		{
			// disable copy constructor and assignment
			ListEx(const ListEx&);
			void operator = (const ListEx&);

		public:
			ListEx()  {}
			~ListEx() {}

			// Must have head pointer
			CastNode* GetHead() { return (CastNode*) m_pHead; }
			const CastNode* GetHead() const { return (const CastNode*) m_pHead; }
			__declspec(property(get=GetHead)) CastNode* _Head;
// 			__declspec(property(get=GetHead)) const CastNode* _Head;

			// Must have tail pointer.
			CastNode* GetTail() { return (CastNode*) m_pTail; }
			const CastNode* GetTail() const { return (const CastNode*) m_pTail; }
			__declspec(property(get=GetTail)) CastNode* _Tail;
// 			__declspec(property(get=GetTail)) const CastNode* _Tail;

			// Must have forward iteration
			void InsertNext(CastNode& node, CastNode& pos) { zInsertNext<true>(node, pos); }
			CastNode* RemoveNext(CastNode& pos) { return (CastNode*) zRemoveNext<true>(pos); }
			// Must also have the head pointer
			void InsertHead(CastNode& node) { zInsertHead<true>(node); }
			CastNode* RemoveHead() { return (CastNode*) zRemoveHead<true>(); }
			void InsertNext(CastNode& node, CastNode* pPos) { zInsertNext<true>(node, pPos); }
			CastNode* RemoveNext(CastNode* pPos) { return (CastNode*) zRemoveNext<true>(pPos); }

			// Must have backward iteration
			void InsertPrev(CastNode& node, CastNode& pos) { zInsertNext<false>(node, pos); }
			CastNode* RemovePrev(CastNode& pos) { return (CastNode*) zRemoveNext<false>(pos); }
			// Must also have the tail pointer
			void InsertTail(CastNode& node) { zInsertHead<false>(node); }
			CastNode* RemoveTail() { return (CastNode*) zRemoveHead<false>(); }
			void InsertPrev(CastNode& node, CastNode* pPos) { zInsertNext<false>(node, pPos); }
			CastNode* RemovePrev(CastNode* pPos) { return (CastNode*) zRemoveNext<false>(pPos); }

			// Must have both iterations.
			void Remove(CastNode& node) { zRemove(node); }
		};

		// Dynamic linked list. Same as the above, plus allocates and deletes elements.
		template <class Eng, class CastNode = Eng::Node>
		class ListDyn : public Impl::InhDyn<ListEx<Eng, CastNode>, CastNode>
		{
			// disable copy constructor and assignment
			ListDyn(const ListDyn&);
			void operator = (const ListDyn&);

		public:
			ListDyn() {}

			// Must have forward iteration
			CastNode* CreateNext(CastNode& pos) { return zCreateNext<true>(pos); }
			void DeleteNext(CastNode& pos) { zDeleteNext<true>(pos); }
			// Must also have the head pointer
			CastNode* CreateHead() { return zCreateHead<true>(); }
			void DeleteHead() { zDeleteHead<true>(); }
			CastNode* CreateNext(CastNode* pPos) { zCreateNext<true>(pPos); }
			void DeleteNext(CastNode* pPos) { zDeleteNext<true>(pPos); }

			// Must have backward iteration
			CastNode* CreatePrev(CastNode& pos) { return zCreateNext<false>(pos); }
			void DeletePrev(CastNode& pos) { zDeleteNext<false>(pos); }
			// Must also have the head pointer
			CastNode* CreateTail() { return zCreateHead<false>(); }
			void DeleteTail() { zDeleteHead<false>(); }
			CastNode* CreatePrev(CastNode* pPos) { zCreateNext<false>(pPos); }
			void DeletePrev(CastNode* pPos) { zDeleteNext<false>(pPos); }

		private:
			// Vise/Versa
			template <bool vise> CastNode* zCreateNext(CastNode& pos) { CastNode* p = new CastNode; zInsertNext<vise>(*p, pos); return p; }
			template <bool vise> void zDeleteNext(CastNode& pos) { delete (CastNode*) zRemoveNext<vise>(pos); }
			template <bool vise> CastNode* zCreateHead() { CastNode* p = new CastNode; zInsertHead<vise>(*p); return p; }
			template <bool vise> void zDeleteHead() { delete (CastNode*) zRemoveHead<vise>(); }
			template <bool vise> CastNode* zCreateNext(CastNode* pPos) { CastNode* p = new CastNode; zInsertNext<vise>(*p, pPos); }
			template <bool vise> void zDeleteNext(CastNode* pPos) { delete (CastNode*) zRemoveNext<vise>(pPos); }
		};

		// Common list engine types
		typedef ListEng<true, true, true, true, void>     ListHT;   // double-link, Head + tail.
		typedef ListEng<true, true, true, true, size_t>   ListHTC;  // same, counted.

		typedef ListEng<true, true, true, false, void>    ListH;    // double-link, head.
		typedef ListEng<true, true, true, false, size_t>  ListHC;   // same, counted.

		typedef ListEng<false, true, true, true, void>    QueueHT;  // forward-link, Head + tail.
		typedef ListEng<false, true, true, true, size_t>  QueueHTC; // same, counted.

		typedef ListEng<false, true, true, false, void>   QueueH;   // forward-link, Head.
		typedef ListEng<false, true, true, false, size_t> QueueHC;  // same, counted.

		// Extended linked list node. Easy cast to a successor type.
		template <class Eng, class CastNode>
		struct NodeEx : public Eng::Node {
			CastNode* GetNext()             { return (CastNode*) m_pNext; }
			const CastNode* GetNext() const { return (const CastNode*) m_pNext; }
			CastNode* GetPrev()             { return (CastNode*) m_pPrev; }
			const CastNode* GetPrev() const { return (const CastNode*) m_pPrev; }
			__declspec(property(get=GetNext)) CastNode* _Next;
// 			__declspec(property(get=GetNext)) const CastNode* _Next;
			__declspec(property(get=GetPrev)) CastNode* _Prev;
// 			__declspec(property(get=GetPrev)) const CastNode* _Prev;
		};

		//////////////////////////////////////////////////////////////////////
		// Hash Table

		// Base Hash engine
		namespace Impl
		{
			template<class KEY, class ARG_KEY>
			struct HashEngBase
			{
				struct Node : public NodeEx<QueueH, Node> { KEY m_Key; };

			protected:
				typedef ListEx<QueueH, Node> List;
				typedef ARG_KEY ARG_KEY;

				List*  m_pHashTable;
				size_t m_nHashTableCount;

				void Insert(Node& node)
				{
					EntryFromKey((ARG_KEY) node.m_Key).InsertHead(node);
				}
				void Remove(Node& node)
				{
					List& list = EntryFromKey((ARG_KEY) node.m_Key);
					Node* pVal = list._Head;
					if (&node == pVal)
						list.RemoveHead();
					else
					{
						while (&node != pVal->_Next)
							pVal = pVal->_Next;
						list.RemoveNext(pVal);
					}
				}

				void Swap(HashEngBase& other)
				{
					swap_t(m_pHashTable, other.m_pHashTable);
					swap_t(m_nHashTableCount, other.m_nHashTableCount);
				}

				void Reset() { ZeroMemory(m_pHashTable, m_nHashTableCount * sizeof(List)); }

			private:
				size_t IndexFromKey(ARG_KEY Key) const
				{
					M_ASSERT(m_pHashTable && m_nHashTableCount);
					return MapInlHashKey(Key) % m_nHashTableCount;
				}
				List& EntryFromKey(ARG_KEY Key)             { return m_pHashTable[IndexFromKey(Key)]; }
				const List& EntryFromKey(ARG_KEY Key) const { return m_pHashTable[IndexFromKey(Key)]; }

			public:
				HashEngBase() : m_pHashTable(NULL) ,m_nHashTableCount(0) {}

				Node* Find(ARG_KEY Key)
				{
					for (Node* pNode = EntryFromKey(Key)._Head; ; pNode = pNode->_Next)
					{
						if (!pNode)
							return NULL;
						if (pNode->m_Key == Key)
							return pNode;
					}
					// unreachable
				}
				const Node* Find(ARG_KEY Key) const
				{
					return ((HashEngBase*) this)->Find(Key);
				}

				// The following is for walking through elements. Not recommended to use, better arrange
				// the elements in an extra list.
				Node* GetFirst()
				{
					if (m_pHashTable)
						for (size_t nIndex = 0; nIndex < m_nHashTableCount; nIndex++)
						{
							Node* pVal = m_pHashTable[nIndex]._Head;
							if (pVal)
								return pVal;
						}
						return NULL; // the map is empty
				}
				Node* GetNext(Node* pNode)
				{
					M_ASSERT(pNode);
					if (pNode->_Next)
						return pNode->_Next;

					for (size_t nIndex = IndexFromKey((ARG_KEY) pNode->m_Key) + 1; nIndex < m_nHashTableCount; nIndex++)
					{
						Node* pVal = m_pHashTable[nIndex]._Head;
						if (pVal)
							return pVal;
					}
					return NULL;
				}
			};

		}; // namespace Impl


		// Hash table engine, either with or without the InhCounter
		template<class KEY, class ARG_KEY, class T_COUNT>
		class HashEng : public Impl::InhCounter<Impl::HashEngBase<KEY, ARG_KEY>, T_COUNT>
		{
			typedef Impl::HashEngBase<KEY, ARG_KEY> Eng;
		protected:
			// should not be instantiated as-is
			HashEng()  {}
			~HashEng() {}

		public:
			typedef typename Eng::Node Node;

			void Reset()
			{
				Eng::Reset();
				zResetCount();
			}
			void Swap(HashEng& other)
			{
				Eng::Swap(other);
				zSwapCount(other);
			}
			void Insert(Node& node)
			{
				Eng::Insert(node);
				zInc();
			}
			void Insert(Node& node, ARG_KEY key)
			{
				node.m_Key = key;
				Insert(node);
			}
			void Remove(Node& node)
			{
				Eng::Remove(node);
				zDec();
			}
		};

		// Hash table, with cast + table placeholder.
		template <class HashEng, size_t nHashTableCount, class CastNode = HashEng::Node>
		class HashEx : public HashEng
		{
			BYTE m_pPlaceHolder[sizeof(HashEng::List) * nHashTableCount];

			// disable copy constructor and assignment
			HashEx(const HashEx&);
			void operator = (const HashEx&);
		public:
			HashEx()
			{
				ZeroMemory(m_pPlaceHolder, sizeof(m_pPlaceHolder));
				m_pHashTable = (HashEng::List*) m_pPlaceHolder;
				m_nHashTableCount = nHashTableCount;
			}

			void Insert(CastNode& node)
			{
				HashEng::Insert(node);
			}
			void Insert(CastNode& node, typename HashEng::ARG_KEY Key)
			{
				HashEng::Insert(node, Key);
			}
			CastNode* Find(typename HashEng::ARG_KEY Key)
			{
				return (CastNode*) HashEng::Find(Key);
			}
			const CastNode* Find(typename HashEng::ARG_KEY Key) const
			{
				return ((HashEx*) this)->Find(Key);
			}
			void Remove(CastNode& node)
			{
				HashEng::Remove(node);
			}
		};

		// Common hash table types
		typedef HashEng<ULONG_PTR, ULONG_PTR, void>   HashOrd;   // integer key.
		typedef HashEng<ULONG_PTR, ULONG_PTR, size_t> HashOrdC;  // same, counted.


		//////////////////////////////////////////////////////////////////////
		// Tree
		namespace Impl
		{
			class TreeEngRaw
			{
			protected:
				class Node {
				protected:

					Node* m_pC[2]; // Left/Right children
					Node* m_pT;
					int m_nBallance; // Negative=Left, Positive=Right

					bool zIsBallanceOk() const { return (m_nBallance >= -1) && (m_nBallance <= 1); }

					int zParentIdx() const
					{
						M_ASSERT((this == m_pT->m_pC[0]) || (this == m_pT->m_pC[1]));
						return (this == m_pT->m_pC[1]);
					}
					void zSetChildSafe(int nIdx, Node* pChild)
					{
						if (m_pC[nIdx] = pChild)
							pChild->m_pT = this;
					}
#ifdef _DEBUG
					size_t zAssertValid(size_t& nTotal, const Node* pT, size_t nL, size_t nR) const
					{
						M_ASSERT(m_pT == pT);
						M_ASSERT(zIsBallanceOk());
						M_ASSERT(nL + m_nBallance == nR);

						nTotal++;
						return max(nL, nR) + 1;
					}
#endif // _DEBUG

					friend class TreeEngRaw;
					template <class KEY, class ARG_KEY>
					friend struct TreeEngBase;
				};

				Node* m_pRoot;

				void zReplaceFixTop(Node& node, Node& next)
				{
					if (next.m_pT = node.m_pT)
						node.m_pT->m_pC[node.zParentIdx()] = &next;
					else
					{
						M_ASSERT(&node == m_pRoot);
						m_pRoot = &next;
					}
				}

				bool zRotate(Node& node, int nDir)
				{
					M_ASSERT((-1 == nDir) || (1 == nDir));
					int nIdx = (1 == nDir);

					M_ASSERT(node.m_nBallance);
					M_ASSERT(node.m_nBallance * nDir < 0);

					Node* pNext = node.m_pC[!nIdx];
					M_ASSERT(pNext && pNext->zIsBallanceOk());

					if (nDir == pNext->m_nBallance)
					{
						M_ASSERT(!zRotate(*pNext, -nDir));
						pNext = pNext = node.m_pC[!nIdx];
						M_ASSERT(pNext && pNext->zIsBallanceOk());
						M_ASSERT(nDir != pNext->m_nBallance);
					}

					bool bDepthDecrease = pNext->m_nBallance && !node.zIsBallanceOk();

					node.m_nBallance += nDir;
					if (pNext->m_nBallance)
					{
						if (!node.m_nBallance)
							pNext->m_nBallance += nDir;
						node.m_nBallance += nDir;
					}
					pNext->m_nBallance += nDir;

					node.zSetChildSafe(!nIdx, pNext->m_pC[nIdx]);

					pNext->m_pC[nIdx] = &node;

					zReplaceFixTop(node, *pNext);

					node.m_pT = pNext;

					return bDepthDecrease;
				}

				void zAdjustBallance(Node* pNode, int nDir, bool bRemoved)
				{
					M_ASSERT((1 == nDir) || (-1 == nDir));

					while (true)
					{
						M_ASSERT(pNode && pNode->zIsBallanceOk());

						Node* pT = pNode->m_pT;
						pNode->m_nBallance += nDir;

						int nDirNext;
						if (pT)
							nDirNext = ((0 != pNode->zParentIdx()) ^ bRemoved) ? 1 : -1;

						bool bMatch;
						switch (pNode->m_nBallance)
						{
						default: M_ASSERT(0);
						case -1:
						case 1:
							bMatch = false;
							break;
						case 0:
							bMatch = true;
							break;

						case -2:
							bMatch = zRotate(*pNode, 1);
							break;

						case 2:
							bMatch = zRotate(*pNode, -1);
							break;
						}

						if (!pT || (bMatch ^ bRemoved))
							break;

						pNode = pT;
						nDir = nDirNext;
					}
				}

				void zRemove(Node& node, Node* pOnlyChild)
				{
					M_ASSERT(!node.m_pC[0] || !node.m_pC[1]);

					if (pOnlyChild)
						pOnlyChild->m_pT = node.m_pT;

					Node* pT = node.m_pT;
					if (pT)
					{
						int nIdx = node.zParentIdx();
						pT->m_pC[nIdx] = pOnlyChild;

						zAdjustBallance(pT, nIdx ? -1 : 1, true);

					} else
						m_pRoot = pOnlyChild;
				}

				template <int nIdx> static Node* zGetExtr(Node* pPos)
				{
					while (true)
					{
						Node* pVal = pPos->m_pC[nIdx];
						if (!pVal)
							return pPos;
						pPos = pVal;
					}
				}
				template <int nIdx> Node* zGetExtrRoot()
				{
					return m_pRoot ? zGetExtr<nIdx>(m_pRoot) : NULL;
				}


				template <int nIdx>
				static Node* zWalk(Node& node)
				{
					Node* pRet = node.m_pC[nIdx];
					if (pRet)
						return zGetExtr<!nIdx>(pRet);

					pRet = &node;
					while (true)
					{
						Node* pParent = pRet->m_pT;
						if (!pParent)
							return NULL;

						if (pRet->zParentIdx() != nIdx)
							return pParent;

						pRet = pParent;
					}
				}

				Node* zAnyOrderRemove()
				{
					Node* pRoot = m_pRoot;
					M_ASSERT(pRoot);
					zRemove(*pRoot);
					return pRoot;
				}

				void Reset() { m_pRoot = NULL; }
				void Swap(TreeEngRaw& other) { swap_t(m_pRoot, other.m_pRoot); }

				TreeEngRaw() :m_pRoot(NULL) {}


				void zRemove(Node& node)
				{
					if (node.m_pC[0])
						if (node.m_pC[1])
						{
							// find the successor of this node.
							Node* pSucc = zGetExtr<0>(node.m_pC[1]);

							zRemove(*pSucc, pSucc->m_pC[1]);

							pSucc->zSetChildSafe(0, node.m_pC[0]);
							pSucc->zSetChildSafe(1, node.m_pC[1]);
							zReplaceFixTop(node, *pSucc);

							pSucc->m_nBallance = node.m_nBallance;

						} else
							zRemove(node, node.m_pC[0]);
					else
						zRemove(node, node.m_pC[1]);
				}

				void zInsert(Node& node, Node* pT, int nIdx)
				{
					node.m_pC[0] = NULL;
					node.m_pC[1] = NULL;
					node.m_pT = pT;
					node.m_nBallance = 0;

					if (pT)
					{
						M_ASSERT(!pT->m_pC[nIdx]);
						pT->m_pC[nIdx] = &node;

						zAdjustBallance(pT, nIdx ? 1 : -1, false);

					} else
					{
						M_ASSERT(!m_pRoot);
						m_pRoot = &node;
					}
				}

			public:
				~TreeEngRaw() { M_ASSERT(IsEmpty()); }
				bool IsEmpty() const { return !m_pRoot; }
				__declspec(property(get=IsEmpty)) bool _Empty;
			};


			template <class KEY, class ARG_KEY>
			struct TreeEngBase : public TreeEngRaw
			{
				typedef ARG_KEY ARG_KEY;
				struct Node : public TreeEngRaw::Node
				{
					KEY m_Key;

#ifdef _DEBUG
				private:
					size_t zAssertValid(size_t& nTotal, const Node* pT, int nDir) const
					{
						if (!this)
							return 0;

						if (pT)
						{
							TreeInlComparator<ARG_KEY> hint(pT->m_Key);
							int nCmp = hint.Compare(m_Key);
							M_ASSERT(!nCmp || (nCmp == nDir));
						}

						size_t nL = ((Node*) m_pC[0])->zAssertValid(nTotal, this, -1);
						size_t nR = ((Node*) m_pC[1])->zAssertValid(nTotal, this, 1);
						return TreeEngRaw::Node::zAssertValid(nTotal, pT, nL, nR);
					}
					friend struct TreeEngBase;
#endif // _DEBUG
				};

			private:
				template <bool bExact, int nDir>
				Node* zFind(ARG_KEY Key)
				{
					TreeInlComparator<ARG_KEY> hint(Key);
					Node* pMatch = NULL;

					for (Node* pNode = (Node*) m_pRoot; pNode; )
					{
						int nCmp = hint.Compare(pNode->m_Key);

						if (bExact && !nCmp)
							return pNode; // exact match.

						if (nCmp == nDir)
							pMatch = pNode;

						pNode = (Node*) pNode->m_pC[nCmp < 0];
					}
					return pMatch;
				}

			public:
				Node* Find(ARG_KEY Key)             { return zFind<true, 0>(Key); }
				Node* FindMin()                     { return (Node*) zGetExtrRoot<0>(); }
				Node* FindMax()                     { return (Node*) zGetExtrRoot<1>(); }
				Node* FindSmaller(ARG_KEY Key)      { return zFind<false, -1>(Key); }
				Node* FindBigger(ARG_KEY Key)       { return zFind<false, 1>(Key); }
				Node* FindExactSmaller(ARG_KEY Key) { return zFind<true, -1>(Key); }
				Node* FindExactBigger(ARG_KEY Key)  { return zFind<true, 1>(Key); }

			protected:
#ifdef _DEBUG
				size_t AssertValid() const
				{
					size_t nTotal = 0;
					((Node*) m_pRoot)->zAssertValid(nTotal, NULL, 0);
					return nTotal;
				}
#endif // _DEBUG
				void zInsert(Node& node)
				{
					if (m_pRoot)
					{
						TreeInlComparator<ARG_KEY> hint(node.m_Key);
						for (Node* pNode = (Node*) m_pRoot; ; )
						{
							int nIdx = (hint.Compare(pNode->m_Key) < 0);
							if (!pNode->m_pC[nIdx])
							{
								TreeEngRaw::zInsert(node, pNode, nIdx);
								break;
							}
							pNode = (Node*) pNode->m_pC[nIdx];
						}
					}
					else
						TreeEngRaw::zInsert(node, NULL, 0);
				}
				void zRemove(Node& node)
				{
					TreeEngRaw::zRemove(node);
				}

				Node* zAnyOrderRemove() { return (Node*) TreeEngRaw::zAnyOrderRemove(); }

				static Node* zWalkL(Node& node) { return (Node*) TreeEngRaw::zWalk<0>(node); }
				static Node* zWalkR(Node& node) { return (Node*) TreeEngRaw::zWalk<1>(node); }

				template <int nIdx>
				static Node* zWalkEqual(Node& node)
				{
					Node* pVal = (Node*) zWalk<nIdx>(node);
					if (pVal)
					{
						TreeInlComparator<ARG_KEY> hint(node.m_Key);
						if (!hint.Compare(pVal->m_Key))
							return pVal;
					}
					return NULL;
				}

				template <int nIdx>
				static Node* zGetExtrEqual(Node* pPos)
				{
					while (true)
					{
						Node* pVal = zWalkEqual<nIdx>(*pPos);
						if (!pVal)
							return pPos;

						pPos = pVal;
					}
					return NULL;
				}

				static Node* zWalkEqualL(Node& node) { return zWalkEqual<0>(node); }
				static Node* zWalkEqualR(Node& node) { return zWalkEqual<1>(node); }

				static Node* zWalkEqualMin(Node& node) { return zGetExtrEqual<0>(&node); }
				static Node* zWalkEqualMax(Node& node) { return zGetExtrEqual<1>(&node); }
			};


		}; // namespace Impl

		template <class KEY, class ARG_KEY, class T_COUNT>
		class TreeEng : public Impl::InhCounter<Impl::TreeEngBase<KEY, ARG_KEY>, T_COUNT>
		{
		protected:
			// should not be instantiated as-is
			TreeEng() {}
			~TreeEng() {}

		public:
			typedef Impl::TreeEngBase<KEY, ARG_KEY> Eng;
			typedef typename Eng::Node Node;

			void Reset()
			{
				Eng::Reset();
				zResetCount();
			}
			void Swap(TreeEng& other)
			{
				Eng::Swap(other);
				zSwapCount(other);
			}
			void Insert(Node& node)
			{
				Eng::zInsert(node);
				zInc();
			}
			void Insert(Node& node, ARG_KEY key)
			{
				node.m_Key = key;
				Insert(node);
			}
			void Remove(Node& node)
			{
				Eng::zRemove(node);
				zDec();
			}
			void AssertValid()
			{
#ifdef _DEBUG
				zAssertCount(Eng::AssertValid());
#endif // _DEBUG
			}

			Node* zAnyOrderRemove() { zDec(); return Eng::zAnyOrderRemove(); }
		};

		// Tree with cast either with or without the count.
		template <class TreeEng, class CastNode = TreeEng::Node>
		class TreeEx : public TreeEng
		{
			// disable copy constructor and assignment
			TreeEx(const TreeEx&);
			void operator = (const TreeEx&);
		public:
			TreeEx() {}

			CastNode* Find(typename TreeEng::ARG_KEY Key)
			{ return (CastNode*) TreeEng::Find(Key); }
			CastNode* FindMin()
			{ return (CastNode*) TreeEng::FindMin(); }
			CastNode* FindMax()
			{ return (CastNode*) TreeEng::FindMax(); }
			CastNode* FindSmaller(typename TreeEng::ARG_KEY Key)
			{ return (CastNode*) TreeEng::FindSmaller(Key); }
			CastNode* FindBigger(typename TreeEng::ARG_KEY Key)
			{ return (CastNode*) TreeEng::FindBigger(Key); }
			CastNode* FindExactSmaller(typename TreeEng::ARG_KEY Key)
			{ return (CastNode*) TreeEng::FindExactSmaller(Key); }
			CastNode* FindExactBigger(typename TreeEng::ARG_KEY Key)
			{ return (CastNode*) TreeEng::FindExactBigger(Key); }
			static CastNode* FindNext(CastNode& node)
			{ return (CastNode*) TreeEng::zWalkR(node); }
			static CastNode* FindPrev(CastNode& node)
			{ return (CastNode*) TreeEng::zWalkL(node); }

			static CastNode* FindNextEqual(CastNode& node)
			{ return (CastNode*) TreeEng::zWalkEqualR(node); }
			static CastNode* FindPrevEqual(CastNode& node)
			{ return (CastNode*) TreeEng::zWalkEqualL(node); }
			static CastNode* FindMinEqual(CastNode& node)
			{ return (CastNode*) TreeEng::zWalkEqualMin(node); }
			static CastNode* FindMaxEqual(CastNode& node)
			{ return (CastNode*) TreeEng::zWalkEqualMax(node); }




			const CastNode* Find(typename TreeEng::ARG_KEY Key) const
			{ return ((TreeEx*) this)->Find(Key); }
			const CastNode* FindMin() const
			{ return ((TreeEx*) this)->FindMin(); }
			const CastNode* FindMax() const
			{ return ((TreeEx*) this)->FindMax(); }
			const CastNode* FindSmaller(typename TreeEng::ARG_KEY Key) const
			{ return ((TreeEx*) this)->FindSmaller(Key); }
			const CastNode* FindBigger(typename TreeEng::ARG_KEY Key) const
			{ return ((TreeEx*) this)->FindBigger(Key); }
			const CastNode* FindExactSmaller(typename TreeEng::ARG_KEY Key) const
			{ return ((TreeEx*) this)->FindExactSmaller(Key); }
			const CastNode* FindExactBigger(typename TreeEng::ARG_KEY Key) const
			{ return ((TreeEx*) this)->FindExactBigger(Key); }
			static const CastNode* FindNext(const CastNode& node)
			{ return (CastNode*) zWalkR((CastNode&) node); }
			static const CastNode* FindPrev(const CastNode& node)
			{ return (CastNode*) zWalkL((CastNode&) node); }

			static const CastNode* FindNextEqual(const CastNode& node)
			{ return (CastNode*) TreeEng::zWalkEqualR(node); }
			static const CastNode* FindPrevEqual(const CastNode& node)
			{ return (CastNode*) TreeEng::zWalkEqualL(node); }
			static const CastNode* FindMinEqual(const CastNode& node)
			{ return (CastNode*) TreeEng::zWalkEqualMin(node); }
			static const CastNode* FindMaxEqual(const CastNode& node)
			{ return (CastNode*) zWalkEqualMax(node); }

			void Insert(CastNode& node)
			{ TreeEng::Insert(node); }
			void Insert(CastNode& node, typename TreeEng::ARG_KEY key)
			{ TreeEng::Insert(node, key); }
			void Remove(CastNode& node)
			{ TreeEng::Remove(node); }
		};

		// Tree with cast either with or without the count.
		template <class TreeEng, class CastNode = TreeEng::Node>
		class TreeDyn : public Impl::InhDyn<TreeEx<TreeEng, CastNode>, CastNode>
		{
			// disable copy constructor and assignment
			TreeDyn(const TreeDyn&);
			void operator = (const TreeDyn&);
		public:
			TreeDyn() {}

			// The following methods demand to have new/delete for the CastNode.
			CastNode* Create(typename TreeEng::ARG_KEY key)
			{
				CastNode* pNode = new CastNode;
				Insert(*pNode, key);
				return pNode;
			}
		};

		// Common hash table types
		typedef TreeEng<ULONG_PTR, ULONG_PTR, void>   TreeOrd;  // integer key.
		typedef TreeEng<ULONG_PTR, ULONG_PTR, size_t> TreeOrdC; // same, counted.

	}; // namespace Container
}
}

#pragma warning (pop) // Restore warnings level.
