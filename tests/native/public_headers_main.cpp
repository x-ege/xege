#ifdef EGE_TEST_ZH_HEADER
void public_headers_zh_smoke();
#else
void public_headers_en_smoke();
#endif

int main() {
#ifdef EGE_TEST_ZH_HEADER
    public_headers_zh_smoke();
#else
    public_headers_en_smoke();
#endif
    return 0;
}
