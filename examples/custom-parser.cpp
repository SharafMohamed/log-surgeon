#include <iostream>
#include <string>
#include <vector>

#include <log_surgeon/CustomParser.hpp>

#include "common.hpp"

using std::string;
using namespace log_surgeon;

/// TODO: json_like_string ending in comma is not handled
auto main() -> int {
    std::string json_like_string
            = " empty=, some_text1, a_random_key1=10, a_random_key2=true, some_text2,"
              " a_random_key3=some_value, some_text3, empty=, a_random_key4=123asd, "
              "a_random_key4==false";
    CustomParser custom_parser;
    std::unique_ptr<JsonRecordAST> json_record_ast1
            = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "AST human readable output:" << json_record_ast1->print(true) << std::endl
              << std::endl;

    json_like_string = "Request and Response Information, SOME_REDUCED_PAYLOAD=null,"
                       " someId=0e820f76-104d-4b1d-b93a-fc1837a63efa, duration=21, bool=true, "
                       "almost-bool2=truefalse, almost-bool2=truer, "
                       "fakeRespHeaders=FA_KE_ID=0:FAKE_LOCALE_ID=en_US:x-o-fake-id=0:FA_KER_ID=0, "
                       "equal==123, equalint=123=123, equalbool=true=false";
    std::unique_ptr<JsonRecordAST> json_record_ast2
            = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "AST human readable output:" << json_record_ast2->print(true) << std::endl
              << std::endl;

    json_like_string
            = "LogFuncArg(level=INFO,log={\\\"traceId\\\":\\\"u\\\",\\\"t\\\":\\\"s/r+qp+on/m/l/k/"
              "j/i+h+gf+e+d/c/b+a/z+y+x+w/vu++t+s/r/q+p+o+n/m/lk/ji/h/gf+ed+c/b/"
              "a\\\"})\",\"Verbosity\":\"INFO\"},\"time\":\"2022-02-22T07:20:20.222222222Z";

    std::unique_ptr<JsonRecordAST> json_record_ast3
            = custom_parser.parse_json_like_string(json_like_string);
    std::cout << "AST human readable output:" << json_record_ast3->print(true) << std::endl
              << std::endl;

    json_like_string
            = "Request and Response Information, SOME_REDUCED_PAYLOAD=null, "
              "someId=590d4d7b-ee6c-400c-bfb9-b14ab0faabe7, duration=22, fakeRespHeaders=FA_KE_ID"
              "=0:FAKE_LOCALE_ID=en_US:x-o-fake-id=0:FA_KER_ID=0, FA_KEBOT_REQ=null, "
              "fakerSourceType=null, "
              "FA_KER_REQUEST=null, ANOTHER_REQUEST=null, FA_KER_MODE=null, fakerCriteria=, "
              "productContextsCriteria=, RGs=PRODUCT"
              "_ASSET:FAKER_INFORMATION:FAKER_INFORMATION, correlationId=58516392, "
              "requestType=Types, "
              "requestIds=YWLtzlTMFFoh, reqCompOfferIds=, reqAssocOfferIds=, "
              "responseIds=nZpf4ZOK8441qRxHXI, "
              "requestPrimaryProductIdMa"
              "p=, fakerIds=6613, status=OK, responseEntitiesSize=5, isSomethingPresent=true, "
              "extFakerCode=null, lat=5.173407, long=-132.458237, addressId=null, fakeCode=53442, "
              "inFakement=null, fakementIntent=, errorMap=,"
              " missingFakementItems=, offerFakedItems=, fakerJarOverrideItems=null, "
              "aFakeLongStringHereAndCount=null, fakeLongStringWithCount=null, "
              "whatJarOverrideItems=, "
              "AddToFakeFalse=, totalOfferCount=3, processedOffe"
              "rCount=2, totalVariantCount=9, addToFakeFalseCount=10, whatOnlyItemCount=8, "
              "offerFakedIndex=, "
              "regFakeCount=3, varFakeCount=6, bvFakeCount=6, bundleFakeCount=10, "
              "aLongFakeStringCount=9, "
              "fakeIdHere=4141, anot"
              "herFakeId=null, isActiveMember=null, isFakeMember=null, isFakeEligibleTrue=, "
              "somethingFakeIds=WKFRjhqkbxfHyYttGZzX11Shgi7FMjmL=ONLINE_ONLY:mL, hereFakeItems=, "
              "mpFakerIdsHere=, missingWrapperItems=, fakeStop"
              "Ids=, fakeFeatureIds=, somethingStoreId=, nonSomethingStoreIds=, accessPoints=, "
              "FAKER_IN_VALIDATION=null, whatFakeTier=null, isFakeBox=true, misMatchFakeCount=10, "
              "fakeFilters=, someRuleFakeSkip=false, fakeE"
              "xps=[], fakeStringIds=QY01z|3a17z|nDoMP|AN0Go|q2wiq, f-a-ker-id=null, whatInfo=, "
              "nothingFakesCount=1, traceparent=e4b56f5c-7af7-4fa0-87d3-48e3f4ebd14b, "
              "originalTraceParent=null, subsFakeReqMap=, rPM=, restr"
              "ictions=, uniqId=87fcf541-5a9b-4174-a94e-10b5e94498dd, gtjot=, vqdt=, sids=, "
              "someFakeCount=6, "
              "someFakeItems=, fakeCountMltb=3, NONFakeItems=, FakeIntegerItems=15391779, "
              "fakeEligibleItems=, FAKE_ISPREVIEW=nu"
              "ll, isSomethingTrigrd=true, bpsEligibleItems=, fa-client-usecase=null, "
              "fakeWordPrice1pOfferCount=8, fakeWordPriceRandOfferCount=1, "
              "fakeWordPrice3pNonWfsOfferCount=6, totalFakerSomethingCount=10, fakeWordSom"
              "ethingSuppressCount=5, cachingObjSize=, oversizedCachingObj=, reqQty=, "
              "pureSOIItems=, "
              "onRollupOvrszdCachingObj=, BCBSElgItems=, nonMemRwdElgItms=, bbEligbleCnt=10, "
              "bbNotEligbleCnt=1, bbNotEligibleBtTrnsctbl"
              "e=2, wupcs=, offerRerankTimedOutCount=10, visionRxItems=, priceDiffCount=7, "
              "priceDiffOffers=, "
              "sellerAnalyticsReqSellerIdCount=10, sellerAnalyticsCachedSellerIdCount=2, "
              "pausedOffersPerSeller={}, buyNowEligib"
              "leItems=YWLtzlTMFFoh, s2sEligibleItems=, isOfferBasedRequest=true, "
              "accept-language=en-US, "
              "x-o-bu=FAKER-US, pauseMisSellerId=[], replItmsCnt=7, psDRct=5, "
              "whatFakeHeaders=e4-bot-signature=null:x-p1troxy-srctr"
              "affic=null, assocBndls=, activeEventTypeIds=, nonMemberOOSItems=, "
              "unAvailableGlobalItems=, "
              "missingGlobalItems=, unAvailableNationalItems=, missingNationalItems=, "
              "unAvailableLastMileCarrierItems=YWLtzlTMFFoh"
              ", unAvailableNationalCarrierItems=YWLtzlTMFFoh, itemIdsWithWalmartStoreOffers=, "
              "eventId=, "
              "memEventItems=, aeItems=, blitzItems=, eaItems=, specialBuyItems=, "
              "isSomethingItems=, "
              "eventIdPreEvent=, winningOffer"
              "Suppressed=6, nonWinningOffersSuppressed=10, winningOfferFCSuppresssedStoreOOS=3, "
              "itemCountForNonWinningOffersTransactable=9, missingProductPublishStatus=, "
              "productUnPublishStatus=, missingOfferPublishStatus"
              "=, offerUnPublishStatus=, missingListingStatusItems=, deListedItems=, "
              "missingPricingItems=, "
              "missingNodeFulfillmentOptionDetails=, missingSpeedDetails=, inEligibleItems=, "
              "missingAvailabilityItems=, unAvailableItems=, ver=1.2.345.6";
    return 0;
}
