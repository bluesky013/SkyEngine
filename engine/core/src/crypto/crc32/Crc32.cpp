//
// Created by Copilot on 2025/3/15.
//

#include <core/crypto/crc32/Crc32.h>
#include <cstring>

// --- Platform detection for hardware CRC32C ---
#if defined(_M_X64) || (defined(__x86_64__) && defined(__SSE4_2__))
    #if defined(_MSC_VER)
        #include <intrin.h>
    #else
        #include <nmmintrin.h>
    #endif
    #define SKY_CRC32C_X86 1
#elif defined(__aarch64__) && defined(__ARM_FEATURE_CRC32)
    #include <arm_acle.h>
    #define SKY_CRC32C_ARM 1
#endif

namespace sky {

    namespace {

        // -----------------------------------------------------------------
        // Fixed lookup tables (constexpr — evaluated at compile time,
        // stored in .rodata; identical to a hand-written literal array)
        // -----------------------------------------------------------------

        static constexpr uint32_t CRC32C_TABLE[256] = { // Castagnoli 0x82F63B78
            0x00000000, 0xF26B8303, 0xE13B70F7, 0x1350F3F4, 0xC79A971F, 0x35F1141C, 0x26A1E7E8, 0xD4CA64EB,
            0x8AD958CF, 0x78B2DBCC, 0x6BE22838, 0x9989AB3B, 0x4D43CFD0, 0xBF284CD3, 0xAC78BF27, 0x5E133C24,
            0x105EC76F, 0xE235446C, 0xF165B798, 0x030E349B, 0xD7C45070, 0x25AFD373, 0x36FF2087, 0xC494A384,
            0x9A879FA0, 0x68EC1CA3, 0x7BBCEF57, 0x89D76C54, 0x5D1D08BF, 0xAF768BBC, 0xBC267848, 0x4E4DFB4B,
            0x20BD8EDE, 0xD2D60DDD, 0xC186FE29, 0x33ED7D2A, 0xE72719C1, 0x154C9AC2, 0x061C6936, 0xF477EA35,
            0xAA64D611, 0x580F5512, 0x4B5FA6E6, 0xB93425E5, 0x6DFE410E, 0x9F95C20D, 0x8CC531F9, 0x7EAEB2FA,
            0x30E349B1, 0xC288CAB2, 0xD1D83946, 0x23B3BA45, 0xF779DEAE, 0x05125DAD, 0x1642AE59, 0xE4292D5A,
            0xBA3A117E, 0x4851927D, 0x5B016189, 0xA96AE28A, 0x7DA08661, 0x8FCB0562, 0x9C9BF696, 0x6EF07595,
            0x417B1DBC, 0xB3109EBF, 0xA0406D4B, 0x522BEE48, 0x86E18AA3, 0x748A09A0, 0x67DAFA54, 0x95B17957,
            0xCBA24573, 0x39C9C670, 0x2A993584, 0xD8F2B687, 0x0C38D26C, 0xFE53516F, 0xED03A29B, 0x1F682198,
            0x5125DAD3, 0xA34E59D0, 0xB01EAA24, 0x42752927, 0x96BF4DCC, 0x64D4CECF, 0x77843D3B, 0x85EFBE38,
            0xDBFC821C, 0x2997011F, 0x3AC7F2EB, 0xC8AC71E8, 0x1C661503, 0xEE0D9600, 0xFD5D65F4, 0x0F36E6F7,
            0x61C69362, 0x93AD1061, 0x80FDE395, 0x72966096, 0xA65C047D, 0x5437877E, 0x4767748A, 0xB50CF789,
            0xEB1FCBAD, 0x197448AE, 0x0A24BB5A, 0xF84F3859, 0x2C855CB2, 0xDEEEDFB1, 0xCDBE2C45, 0x3FD5AF46,
            0x7198540D, 0x83F3D70E, 0x90A324FA, 0x62C8A7F9, 0xB602C312, 0x44694011, 0x5739B3E5, 0xA55230E6,
            0xFB410CC2, 0x092A8FC1, 0x1A7A7C35, 0xE811FF36, 0x3CDB9BDD, 0xCEB018DE, 0xDDE0EB2A, 0x2F8B6829,
            0x82F63B78, 0x709DB87B, 0x63CD4B8F, 0x91A6C88C, 0x456CAC67, 0xB7072F64, 0xA457DC90, 0x563C5F93,
            0x082F63B7, 0xFA44E0B4, 0xE9141340, 0x1B7F9043, 0xCFB5F4A8, 0x3DDE77AB, 0x2E8E845F, 0xDCE5075C,
            0x92A8FC17, 0x60C37F14, 0x73938CE0, 0x81F80FE3, 0x55326B08, 0xA759E80B, 0xB4091BFF, 0x466298FC,
            0x1871A4D8, 0xEA1A27DB, 0xF94AD42F, 0x0B21572C, 0xDFEB33C7, 0x2D80B0C4, 0x3ED04330, 0xCCBBC033,
            0xA24BB5A6, 0x502036A5, 0x4370C551, 0xB11B4652, 0x65D122B9, 0x97BAA1BA, 0x84EA524E, 0x7681D14D,
            0x2892ED69, 0xDAF96E6A, 0xC9A99D9E, 0x3BC21E9D, 0xEF087A76, 0x1D63F975, 0x0E330A81, 0xFC588982,
            0xB21572C9, 0x407EF1CA, 0x532E023E, 0xA145813D, 0x758FE5D6, 0x87E466D5, 0x94B49521, 0x66DF1622,
            0x38CC2A06, 0xCAA7A905, 0xD9F75AF1, 0x2B9CD9F2, 0xFF56BD19, 0x0D3D3E1A, 0x1E6DCDEE, 0xEC064EED,
            0xC38D26C4, 0x31E6A5C7, 0x22B65633, 0xD0DDD530, 0x0417B1DB, 0xF67C32D8, 0xE52CC12C, 0x1747422F,
            0x49547E0B, 0xBB3FFD08, 0xA86F0EFC, 0x5A048DFF, 0x8ECEE914, 0x7CA56A17, 0x6FF599E3, 0x9D9E1AE0,
            0xD3D3E1AB, 0x21B862A8, 0x32E8915C, 0xC083125F, 0x144976B4, 0xE622F5B7, 0xF5720643, 0x07198540,
            0x590AB964, 0xAB613A67, 0xB831C993, 0x4A5A4A90, 0x9E902E7B, 0x6CFBAD78, 0x7FAB5E8C, 0x8DC0DD8F,
            0xE330A81A, 0x115B2B19, 0x020BD8ED, 0xF0605BEE, 0x24AA3F05, 0xD6C1BC06, 0xC5914FF2, 0x37FACCF1,
            0x69E9F0D5, 0x9B8273D6, 0x88D28022, 0x7AB90321, 0xAE7367CA, 0x5C18E4C9, 0x4F48173D, 0xBD23943E,
            0xF36E6F75, 0x0105EC76, 0x12551F82, 0xE03E9C81, 0x34F4F86A, 0xC69F7B69, 0xD5CF889D, 0x27A40B9E,
            0x79B737BA, 0x8BDCB4B9, 0x988C474D, 0x6AE7C44E, 0xBE2DA0A5, 0x4C4623A6, 0x5F16D052, 0xAD7D5351,
        };

        static constexpr uint32_t CRC32_TABLE[256] = { // IEEE 0xEDB88320
            0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
            0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
            0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
            0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
            0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
            0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
            0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
            0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
            0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
            0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
            0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
            0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
            0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
            0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
            0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
            0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
            0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
            0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
            0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
            0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
            0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
            0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
            0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
            0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
            0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
            0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
            0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
            0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
            0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
            0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
            0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
            0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
        };

        // -----------------------------------------------------------------
        // Software fallback
        // -----------------------------------------------------------------

        inline uint32_t SoftwareCrc(const uint32_t *table, const uint8_t *data, size_t size, uint32_t crc)
        {
            crc = ~crc;
            for (size_t i = 0; i < size; ++i) {
                crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
            }
            return ~crc;
        }

        // -----------------------------------------------------------------
        // Hardware CRC32C  (SSE4.2 / ARM CRC)
        //
        // 3-way parallel processing with 4096-byte blocks.
        // The CRC32 instruction has 3-cycle latency / 1-cycle throughput,
        // so 3 independent streams fully hide the pipeline latency.
        // Skip tables provide O(1) CRC combination.
        // -----------------------------------------------------------------

#if defined(SKY_CRC32C_X86) || defined(SKY_CRC32C_ARM)

        static constexpr size_t BLOCK  = 4096;          // Tier 0 segment
        static constexpr size_t BLOCK1 = 1360;          // Tier 1 segment
        static constexpr size_t BLOCK2 = 336;            // Tier 2 segment
        static constexpr size_t PREFETCH_HORIZON = 256;  // bytes ahead

        // Pre-computed skip tables for combining 3 parallel CRC streams.
        // Tier 0: SKIP_1X / SKIP_2X for BLOCK (4096 / 8192)
        static constexpr uint32_t SKIP_1X[8][16] = {
            {0x00000000, 0xC2A5B65E, 0x80A71A4D, 0x4202AC13, 0x04A2426B, 0xC607F435, 0x84055826, 0x46A0EE78, 0x094484D6, 0xCBE13288, 0x89E39E9B, 0x4B4628C5, 0x0DE6C6BD, 0xCF4370E3, 0x8D41DCF0, 0x4FE46AAE},
            {0x00000000, 0x128909AC, 0x25121358, 0x379B1AF4, 0x4A2426B0, 0x58AD2F1C, 0x6F3635E8, 0x7DBF3C44, 0x94484D60, 0x86C144CC, 0xB15A5E38, 0xA3D35794, 0xDE6C6BD0, 0xCCE5627C, 0xFB7E7888, 0xE9F77124},
            {0x00000000, 0x2D7CEC31, 0x5AF9D862, 0x77853453, 0xB5F3B0C4, 0x988F5CF5, 0xEF0A68A6, 0xC2768497, 0x6E0B1779, 0x4377FB48, 0x34F2CF1B, 0x198E232A, 0xDBF8A7BD, 0xF6844B8C, 0x81017FDF, 0xAC7D93EE},
            {0x00000000, 0xDC162EF2, 0xBDC02B15, 0x61D605E7, 0x7E6C20DB, 0xA27A0E29, 0xC3AC0BCE, 0x1FBA253C, 0xFCD841B6, 0x20CE6F44, 0x41186AA3, 0x9D0E4451, 0x82B4616D, 0x5EA24F9F, 0x3F744A78, 0xE362648A},
            {0x00000000, 0xFC5CF59D, 0xFD559DCB, 0x01096856, 0xFF474D67, 0x031BB8FA, 0x0212D0AC, 0xFE4E2531, 0xFB62EC3F, 0x073E19A2, 0x063771F4, 0xFA6B8469, 0x0425A158, 0xF87954C5, 0xF9703C93, 0x052CC90E},
            {0x00000000, 0xF329AE8F, 0xE3BF2BEF, 0x10968560, 0xC292212F, 0x31BB8FA0, 0x212D0AC0, 0xD204A44F, 0x80C834AF, 0x73E19A20, 0x63771F40, 0x905EB1CF, 0x425A1580, 0xB173BB0F, 0xA1E53E6F, 0x52CC90E0},
            {0x00000000, 0x047C1FAF, 0x08F83F5E, 0x0C8420F1, 0x11F07EBC, 0x158C6113, 0x190841E2, 0x1D745E4D, 0x23E0FD78, 0x279CE2D7, 0x2B18C226, 0x2F64DD89, 0x321083C4, 0x366C9C6B, 0x3AE8BC9A, 0x3E94A335},
            {0x00000000, 0x47C1FAF0, 0x8F83F5E0, 0xC8420F10, 0x1AEB9D31, 0x5D2A67C1, 0x956868D1, 0xD2A99221, 0x35D73A62, 0x7216C092, 0xBA54CF82, 0xFD953572, 0x2F3CA753, 0x68FD5DA3, 0xA0BF52B3, 0xE77EA843}
        };

        static constexpr uint32_t SKIP_2X[8][16] = {
            {0x00000000, 0xE040E0AC, 0xC56DB7A9, 0x252D5705, 0x8F3719A3, 0x6F77F90F, 0x4A5AAE0A, 0xAA1A4EA6, 0x1B8245B7, 0xFBC2A51B, 0xDEEFF21E, 0x3EAF12B2, 0x94B55C14, 0x74F5BCB8, 0x51D8EBBD, 0xB1980B11},
            {0x00000000, 0x37048B6E, 0x6E0916DC, 0x590D9DB2, 0xDC122DB8, 0xEB16A6D6, 0xB21B3B64, 0x851FB00A, 0xBDC82D81, 0x8ACCA6EF, 0xD3C13B5D, 0xE4C5B033, 0x61DA0039, 0x56DE8B57, 0x0FD316E5, 0x38D79D8B},
            {0x00000000, 0x7E7C2DF3, 0xFCF85BE6, 0x82847615, 0xFC1CC13D, 0x8260ECCE, 0x00E49ADB, 0x7E98B728, 0xFDD5F48B, 0x83A9D978, 0x012DAF6D, 0x7F51829E, 0x01C935B6, 0x7FB51845, 0xFD316E50, 0x834D43A3},
            {0x00000000, 0xFE479FE7, 0xF963493F, 0x0724D6D8, 0xF72AE48F, 0x096D7B68, 0x0E49ADB0, 0xF00E3257, 0xEBB9BFEF, 0x15FE2008, 0x12DAF6D0, 0xEC9D6937, 0x1C935B60, 0xE2D4C487, 0xE5F0125F, 0x1BB78DB8},
            {0x00000000, 0xD29F092F, 0xA0D264AF, 0x724D6D80, 0x4448BFAF, 0x96D7B680, 0xE49ADB00, 0x3605D22F, 0x88917F5E, 0x5A0E7671, 0x28431BF1, 0xFADC12DE, 0xCCD9C0F1, 0x1E46C9DE, 0x6C0BA45E, 0xBE94AD71},
            {0x00000000, 0x14CE884D, 0x299D109A, 0x3D5398D7, 0x533A2134, 0x47F4A979, 0x7AA731AE, 0x6E69B9E3, 0xA6744268, 0xB2BACA25, 0x8FE952F2, 0x9B27DABF, 0xF54E635C, 0xE180EB11, 0xDCD373C6, 0xC81DFB8B},
            {0x00000000, 0x4904F221, 0x9209E442, 0xDB0D1663, 0x21FFBE75, 0x68FB4C54, 0xB3F65A37, 0xFAF2A816, 0x43FF7CEA, 0x0AFB8ECB, 0xD1F698A8, 0x98F26A89, 0x6200C29F, 0x2B0430BE, 0xF00926DD, 0xB90DD4FC},
            {0x00000000, 0x87FEF9D4, 0x0A118559, 0x8DEF7C8D, 0x14230AB2, 0x93DDF366, 0x1E328FEB, 0x99CC763F, 0x28461564, 0xAFB8ECB0, 0x2257903D, 0xA5A969E9, 0x3C651FD6, 0xBB9BE602, 0x36749A8F, 0xB18A635B}
        };

        // Tier 1: SKIP1_1X / SKIP1_2X for BLOCK1 (1360 / 2720)
        static constexpr uint32_t SKIP1_1X[8][16] = {
            {0x00000000, 0x79113270, 0xF22264E0, 0x8B335690, 0xE1A8BF31, 0x98B98D41, 0x138ADBD1, 0x6A9BE9A1, 0xC6BD0893, 0xBFAC3AE3, 0x349F6C73, 0x4D8E5E03, 0x2715B7A2, 0x5E0485D2, 0xD537D342, 0xAC26E132},
            {0x00000000, 0x889667D7, 0x14C0B95F, 0x9C56DE88, 0x298172BE, 0xA1171569, 0x3D41CBE1, 0xB5D7AC36, 0x5302E57C, 0xDB9482AB, 0x47C25C23, 0xCF543BF4, 0x7A8397C2, 0xF215F015, 0x6E432E9D, 0xE6D5494A},
            {0x00000000, 0xA605CAF8, 0x49E7E301, 0xEFE229F9, 0x93CFC602, 0x35CA0CFA, 0xDA282503, 0x7C2DEFFB, 0x2273FAF5, 0x8476300D, 0x6B9419F4, 0xCD91D30C, 0xB1BC3CF7, 0x17B9F60F, 0xF85BDFF6, 0x5E5E150E},
            {0x00000000, 0x44E7F5EA, 0x89CFEBD4, 0xCD281E3E, 0x1673A159, 0x529454B3, 0x9FBC4A8D, 0xDB5BBF67, 0x2CE742B2, 0x6800B758, 0xA528A966, 0xE1CF5C8C, 0x3A94E3EB, 0x7E731601, 0xB35B083F, 0xF7BCFDD5},
            {0x00000000, 0x59CE8564, 0xB39D0AC8, 0xEA538FAC, 0x62D66361, 0x3B18E605, 0xD14B69A9, 0x8885ECCD, 0xC5ACC6C2, 0x9C6243A6, 0x7631CC0A, 0x2FFF496E, 0xA77AA5A3, 0xFEB420C7, 0x14E7AF6B, 0x4D292A0F},
            {0x00000000, 0x8EB5FB75, 0x1887801B, 0x96327B6E, 0x310F0036, 0xBFBAFB43, 0x2988802D, 0xA73D7B58, 0x621E006C, 0xECABFB19, 0x7A998077, 0xF42C7B02, 0x5311005A, 0xDDA4FB2F, 0x4B968041, 0xC5237B34},
            {0x00000000, 0xC43C00D8, 0x8D947741, 0x49A87799, 0x1EC49873, 0xDAF898AB, 0x9350EF32, 0x576CEFEA, 0x3D8930E6, 0xF9B5303E, 0xB01D47A7, 0x7421477F, 0x234DA895, 0xE771A84D, 0xAED9DFD4, 0x6AE5DF0C},
            {0x00000000, 0x7B1261CC, 0xF624C398, 0x8D36A254, 0xE9A5F1C1, 0x92B7900D, 0x1F813259, 0x64935395, 0xD6A79573, 0xADB5F4BF, 0x208356EB, 0x5B913727, 0x3F0264B2, 0x4410057E, 0xC926A72A, 0xB234C6E6}
        };

        static constexpr uint32_t SKIP1_2X[8][16] = {
            {0x00000000, 0x7B454CB3, 0xF68A9966, 0x8DCFD5D5, 0xE8F9443D, 0x93BC088E, 0x1E73DD5B, 0x653691E8, 0xD41EFE8B, 0xAF5BB238, 0x229467ED, 0x59D12B5E, 0x3CE7BAB6, 0x47A2F605, 0xCA6D23D0, 0xB1286F63},
            {0x00000000, 0xADD18BE7, 0x5E4F613F, 0xF39EEAD8, 0xBC9EC27E, 0x114F4999, 0xE2D1A341, 0x4F0028A6, 0x7CD1F20D, 0xD10079EA, 0x229E9332, 0x8F4F18D5, 0xC04F3073, 0x6D9EBB94, 0x9E00514C, 0x33D1DAAB},
            {0x00000000, 0xF9A3E41A, 0xF6ABBEC5, 0x0F085ADF, 0xE8BB0B7B, 0x1118EF61, 0x1E10B5BE, 0xE7B351A4, 0xD49A6007, 0x2D39841D, 0x2231DEC2, 0xDB923AD8, 0x3C216B7C, 0xC5828F66, 0xCA8AD5B9, 0x332931A3},
            {0x00000000, 0xACD8B6FF, 0x5C5D1B0F, 0xF085ADF0, 0xB8BA361E, 0x146280E1, 0xE4E72D11, 0x483F9BEE, 0x74981ACD, 0xD840AC32, 0x28C501C2, 0x841DB73D, 0xCC222CD3, 0x60FA9A2C, 0x907F37DC, 0x3CA78123},
            {0x00000000, 0xE930359A, 0xD78C1DC5, 0x3EBC285F, 0xAAF44D7B, 0x43C478E1, 0x7D7850BE, 0x94486524, 0x5004EC07, 0xB934D99D, 0x8788F1C2, 0x6EB8C458, 0xFAF0A17C, 0x13C094E6, 0x2D7CBCB9, 0xC44C8923},
            {0x00000000, 0xA009D80E, 0x45FFC6ED, 0xE5F61EE3, 0x8BFF8DDA, 0x2BF655D4, 0xCE004B37, 0x6E099339, 0x12136D45, 0xB21AB54B, 0x57ECABA8, 0xF7E573A6, 0x99ECE09F, 0x39E53891, 0xDC132672, 0x7C1AFE7C},
            {0x00000000, 0x2426DA8A, 0x484DB514, 0x6C6B6F9E, 0x909B6A28, 0xB4BDB0A2, 0xD8D6DF3C, 0xFCF005B6, 0x24DAA2A1, 0x00FC782B, 0x6C9717B5, 0x48B1CD3F, 0xB441C889, 0x90671203, 0xFC0C7D9D, 0xD82AA717},
            {0x00000000, 0x49B54542, 0x936A8A84, 0xDADFCFC6, 0x233963F9, 0x6A8C26BB, 0xB053E97D, 0xF9E6AC3F, 0x4672C7F2, 0x0FC782B0, 0xD5184D76, 0x9CAD0834, 0x654BA40B, 0x2CFEE149, 0xF6212E8F, 0xBF946BCD}
        };

        // Tier 2: SKIP2_1X / SKIP2_2X for BLOCK2 (336 / 672)
        static constexpr uint32_t SKIP2_1X[8][16] = {
            {0x00000000, 0x8F158014, 0x1BC776D9, 0x94D2F6CD, 0x378EEDB2, 0xB89B6DA6, 0x2C499B6B, 0xA35C1B7F, 0x6F1DDB64, 0xE0085B70, 0x74DAADBD, 0xFBCF2DA9, 0x589336D6, 0xD786B6C2, 0x4354400F, 0xCC41C01B},
            {0x00000000, 0xDE3BB6C8, 0xB99B1B61, 0x67A0ADA9, 0x76DA4033, 0xA8E1F6FB, 0xCF415B52, 0x117AED9A, 0xEDB48066, 0x338F36AE, 0x542F9B07, 0x8A142DCF, 0x9B6EC055, 0x4555769D, 0x22F5DB34, 0xFCCE6DFC},
            {0x00000000, 0xDE85763D, 0xB8E69A8B, 0x6663ECB6, 0x742143E7, 0xAAA435DA, 0xCCC7D96C, 0x1242AF51, 0xE84287CE, 0x36C7F1F3, 0x50A41D45, 0x8E216B78, 0x9C63C429, 0x42E6B214, 0x24855EA2, 0xFA00289F},
            {0x00000000, 0xD569796D, 0xAF3E842B, 0x7A57FD46, 0x5B917EA7, 0x8EF807CA, 0xF4AFFA8C, 0x21C683E1, 0xB722FD4E, 0x624B8423, 0x181C7965, 0xCD750008, 0xECB383E9, 0x39DAFA84, 0x438D07C2, 0x96E47EAF},
            {0x00000000, 0x6BA98C6D, 0xD75318DA, 0xBCFA94B7, 0xAB4A4745, 0xC0E3CB28, 0x7C195F9F, 0x17B0D3F2, 0x5378F87B, 0x38D17416, 0x842BE0A1, 0xEF826CCC, 0xF832BF3E, 0x939B3353, 0x2F61A7E4, 0x44C82B89},
            {0x00000000, 0xA6F1F0F6, 0x480F971D, 0xEEFE67EB, 0x901F2E3A, 0x36EEDECC, 0xD810B927, 0x7EE149D1, 0x25D22A85, 0x8323DA73, 0x6DDDBD98, 0xCB2C4D6E, 0xB5CD04BF, 0x133CF449, 0xFDC293A2, 0x5B336354},
            {0x00000000, 0x4BA4550A, 0x9748AA14, 0xDCECFF1E, 0x2B7D22D9, 0x60D977D3, 0xBC3588CD, 0xF791DDC7, 0x56FA45B2, 0x1D5E10B8, 0xC1B2EFA6, 0x8A16BAAC, 0x7D87676B, 0x36233261, 0xEACFCD7F, 0xA16B9875},
            {0x00000000, 0xADF48B64, 0x5E056039, 0xF3F1EB5D, 0xBC0AC072, 0x11FE4B16, 0xE20FA04B, 0x4FFB2B2F, 0x7DF9F615, 0xD00D7D71, 0x23FC962C, 0x8E081D48, 0xC1F33667, 0x6C07BD03, 0x9FF6565E, 0x3202DD3A}
        };

        static constexpr uint32_t SKIP2_2X[8][16] = {
            {0x00000000, 0xE417F38A, 0xCDC391E5, 0x29D4626F, 0x9E6B553B, 0x7A7CA6B1, 0x53A8C4DE, 0xB7BF3754, 0x393ADC87, 0xDD2D2F0D, 0xF4F94D62, 0x10EEBEE8, 0xA75189BC, 0x43467A36, 0x6A921859, 0x8E85EBD3},
            {0x00000000, 0x7275B90E, 0xE4EB721C, 0x969ECB12, 0xCC3A92C9, 0xBE4F2BC7, 0x28D1E0D5, 0x5AA459DB, 0x9D995363, 0xEFECEA6D, 0x7972217F, 0x0B079871, 0x51A3C1AA, 0x23D678A4, 0xB548B3B6, 0xC73D0AB8},
            {0x00000000, 0x3EDED037, 0x7DBDA06E, 0x43637059, 0xFB7B40DC, 0xC5A590EB, 0x86C6E0B2, 0xB8183085, 0xF31AF749, 0xCDC4277E, 0x8EA75727, 0xB0798710, 0x0861B795, 0x36BF67A2, 0x75DC17FB, 0x4B02C7CC},
            {0x00000000, 0xE3D99863, 0xC25F4637, 0x2186DE54, 0x8152FA9F, 0x628B62FC, 0x430DBCA8, 0xA0D424CB, 0x074983CF, 0xE4901BAC, 0xC516C5F8, 0x26CF5D9B, 0x861B7950, 0x65C2E133, 0x44443F67, 0xA79DA704},
            {0x00000000, 0x0E93079E, 0x1D260F3C, 0x13B508A2, 0x3A4C1E78, 0x34DF19E6, 0x276A1144, 0x29F916DA, 0x74983CF0, 0x7A0B3B6E, 0x69BE33CC, 0x672D3452, 0x4ED42288, 0x40472516, 0x53F22DB4, 0x5D612A2A},
            {0x00000000, 0xE93079E0, 0xD78C8531, 0x3EBCFCD1, 0xAAF57C93, 0x43C50573, 0x7D79F9A2, 0x94498042, 0x50068FD7, 0xB936F637, 0x878A0AE6, 0x6EBA7306, 0xFAF3F344, 0x13C38AA4, 0x2D7F7675, 0xC44F0F95},
            {0x00000000, 0xA00D1FAE, 0x45F649AD, 0xE5FB5603, 0x8BEC935A, 0x2BE18CF4, 0xCE1ADAF7, 0x6E17C559, 0x12355045, 0xB2384FEB, 0x57C319E8, 0xF7CE0646, 0x99D9C31F, 0x39D4DCB1, 0xDC2F8AB2, 0x7C22951C},
            {0x00000000, 0x246AA08A, 0x48D54114, 0x6CBFE19E, 0x91AA8228, 0xB5C022A2, 0xD97FC33C, 0xFD1563B6, 0x26B972A1, 0x02D3D22B, 0x6E6C33B5, 0x4A06933F, 0xB713F089, 0x93795003, 0xFFC6B19D, 0xDBAC1117}
        };

        // O(1) combine: shift a CRC by N zero bytes via skip table
        inline uint32_t ApplySkipTable(const uint32_t table[8][16], uint32_t crc)
        {
            return table[0][ crc        & 0xF] ^ table[1][(crc >>  4) & 0xF] ^
                   table[2][(crc >>  8) & 0xF] ^ table[3][(crc >> 12) & 0xF] ^
                   table[4][(crc >> 16) & 0xF] ^ table[5][(crc >> 20) & 0xF] ^
                   table[6][(crc >> 24) & 0xF] ^ table[7][(crc >> 28) & 0xF];
        }

        // Prefetch helper
        inline void CrcPrefetch(const uint8_t *p)
        {
#if defined(SKY_CRC32C_X86)
    #if defined(_MSC_VER)
            _mm_prefetch(reinterpret_cast<const char *>(p), _MM_HINT_T0);
    #else
            __builtin_prefetch(p, 0, 3);
    #endif
#elif defined(SKY_CRC32C_ARM)
            __builtin_prefetch(p, 0, 3);
#endif
        }

        // 3-way CRC step: process 8 bytes from each of 3 streams
#if defined(SKY_CRC32C_X86)
    #define SKY_CRC_STEP8X3(c0, c1, c2, p0, p1, p2)                  \
        do {                                                           \
            uint64_t v0_, v1_, v2_;                                    \
            memcpy(&v0_, (p0), 8);                                     \
            memcpy(&v1_, (p1), 8);                                     \
            memcpy(&v2_, (p2), 8);                                     \
            (c0) = static_cast<uint32_t>(_mm_crc32_u64((c0), v0_));    \
            (c1) = static_cast<uint32_t>(_mm_crc32_u64((c1), v1_));    \
            (c2) = static_cast<uint32_t>(_mm_crc32_u64((c2), v2_));    \
            (p0) += 8; (p1) += 8; (p2) += 8;                          \
        } while (0)
#else
    #define SKY_CRC_STEP8X3(c0, c1, c2, p0, p1, p2)                  \
        do {                                                           \
            uint64_t v0_, v1_, v2_;                                    \
            memcpy(&v0_, (p0), 8);                                     \
            memcpy(&v1_, (p1), 8);                                     \
            memcpy(&v2_, (p2), 8);                                     \
            (c0) = __crc32cd((c0), v0_);                               \
            (c1) = __crc32cd((c1), v1_);                               \
            (c2) = __crc32cd((c2), v2_);                               \
            (p0) += 8; (p1) += 8; (p2) += 8;                          \
        } while (0)
#endif

        // Single-stream hardware CRC32C (for tail / small data)
        inline uint32_t HardwareCrc32CTail(const uint8_t *data, size_t size, uint32_t crc)
        {
            while (size >= 8) {
                uint64_t v;
                memcpy(&v, data, sizeof(v));
#if defined(SKY_CRC32C_X86)
                crc = static_cast<uint32_t>(_mm_crc32_u64(crc, v));
#else
                crc = __crc32cd(crc, v);
#endif
                data += 8;
                size -= 8;
            }

            if (size >= 4) {
                uint32_t v;
                memcpy(&v, data, sizeof(v));
#if defined(SKY_CRC32C_X86)
                crc = _mm_crc32_u32(crc, v);
#else
                crc = __crc32cw(crc, v);
#endif
                data += 4;
                size -= 4;
            }

            while (size > 0) {
#if defined(SKY_CRC32C_X86)
                crc = _mm_crc32_u8(crc, *data);
#else
                crc = __crc32cb(crc, *data);
#endif
                ++data;
                --size;
            }
            return crc;
        }

        inline uint32_t HardwareCrc32C(const uint8_t *data, size_t size, uint32_t crc)
        {
            crc = ~crc;

            // 3-way parallel: process 3 × BLOCK bytes per iteration
            while (size >= 3 * BLOCK) {
                uint32_t crc0 = crc;
                uint32_t crc1 = 0;
                uint32_t crc2 = 0;

                const uint8_t *p0 = data;
                const uint8_t *p1 = data + BLOCK;
                const uint8_t *p2 = data + BLOCK * 2;
                const uint8_t *end = p0 + BLOCK;

                // Inner loop: 8× unrolled (64 bytes per stream per iteration)
                while (p0 < end) {
                    CrcPrefetch(p0 + PREFETCH_HORIZON);
                    CrcPrefetch(p1 + PREFETCH_HORIZON);
                    CrcPrefetch(p2 + PREFETCH_HORIZON);
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                }

                crc = ApplySkipTable(SKIP_2X, crc0) ^ ApplySkipTable(SKIP_1X, crc1) ^ crc2;

                data += 3 * BLOCK;
                size -= 3 * BLOCK;
            }

            // Tier 1: 3-way parallel, BLOCK1=1360, no unroll
            while (size >= 3 * BLOCK1) {
                uint32_t crc0 = crc;
                uint32_t crc1 = 0;
                uint32_t crc2 = 0;

                const uint8_t *p0 = data;
                const uint8_t *p1 = data + BLOCK1;
                const uint8_t *p2 = data + BLOCK1 * 2;
                const uint8_t *end = p0 + BLOCK1;

                while (p0 < end) {
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                }

                crc = ApplySkipTable(SKIP1_2X, crc0) ^ ApplySkipTable(SKIP1_1X, crc1) ^ crc2;

                data += 3 * BLOCK1;
                size -= 3 * BLOCK1;
            }

            // Tier 2: 3-way parallel, BLOCK2=336, no unroll
            while (size >= 3 * BLOCK2) {
                uint32_t crc0 = crc;
                uint32_t crc1 = 0;
                uint32_t crc2 = 0;

                const uint8_t *p0 = data;
                const uint8_t *p1 = data + BLOCK2;
                const uint8_t *p2 = data + BLOCK2 * 2;
                const uint8_t *end = p0 + BLOCK2;

                while (p0 < end) {
                    SKY_CRC_STEP8X3(crc0, crc1, crc2, p0, p1, p2);
                }

                crc = ApplySkipTable(SKIP2_2X, crc0) ^ ApplySkipTable(SKIP2_1X, crc1) ^ crc2;

                data += 3 * BLOCK2;
                size -= 3 * BLOCK2;
            }

            // Handle remaining bytes (<1008) with single-stream
            crc = HardwareCrc32CTail(data, size, crc);

            return ~crc;
        }

#undef SKY_CRC_STEP8X3

#endif

    } // anonymous namespace

    // -----------------------------------------------------------------
    // Public API — CRC32C
    // -----------------------------------------------------------------

    uint32_t Crc32C(const uint8_t *data, size_t size)
    {
#if defined(SKY_CRC32C_X86) || defined(SKY_CRC32C_ARM)
        return HardwareCrc32C(data, size, 0);
#else
        return SoftwareCrc(CRC32C_TABLE, data, size, 0);
#endif
    }

    uint32_t Crc32C(const uint8_t *data, size_t size, uint32_t crc)
    {
#if defined(SKY_CRC32C_X86) || defined(SKY_CRC32C_ARM)
        return HardwareCrc32C(data, size, crc);
#else
        return SoftwareCrc(CRC32C_TABLE, data, size, crc);
#endif
    }

    // -----------------------------------------------------------------
    // Public API — CRC32 IEEE (software only, no HW instruction)
    // -----------------------------------------------------------------

    uint32_t CalcCrc32(const uint8_t *data, size_t size)
    {
        return SoftwareCrc(CRC32_TABLE, data, size, 0);
    }

    uint32_t CalcCrc32(const uint8_t *data, size_t size, uint32_t crc)
    {
        return SoftwareCrc(CRC32_TABLE, data, size, crc);
    }

} // namespace sky
